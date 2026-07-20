-- InfusionForge: drag-and-drop infusion window for the mod-item-infusion
-- server module. All math is server-side; this UI only previews and asks.
--
-- Protocol (see ItemInfusionAddonMsg.h):
--   send    IFUSE\tP\t<tinv>\t<dbag>\t<dslot>\t<coins>\t<submask>  preview
--           IFUSE\tX\t<same>                                       execute
--   receive R\tH\t<risk>\t<base>\t<penalty>\t<mixPts>\t<nativePts>
--           R\tY\t<prop>:<amount>;...      (may span several messages)
--           R\tS\t<idx>:<itemId>:<red>:<eligible>;...
--           R\tE\t<code>   X\tOK / X\tDEAD / X\tERR\t<code>   O (open)
-- Transport: addon whisper to self, swallowed server-side.

local PREFIX = "IFUSE"

local PROPERTY_NAMES = {
    [0]   = "Mana",
    [1]   = HEALTH or "Health",
    [3]   = SPELL_STAT2_NAME or "Agility",
    [4]   = SPELL_STAT1_NAME or "Strength",
    [5]   = SPELL_STAT4_NAME or "Intellect",
    [6]   = SPELL_STAT5_NAME or "Spirit",
    [7]   = SPELL_STAT3_NAME or "Stamina",
    [12]  = "Defense Rating",
    [13]  = "Dodge Rating",
    [14]  = "Parry Rating",
    [15]  = "Block Rating",
    [16]  = "Hit Rating (Melee)",
    [17]  = "Hit Rating (Ranged)",
    [18]  = "Hit Rating (Spell)",
    [19]  = "Crit Rating (Melee)",
    [20]  = "Crit Rating (Ranged)",
    [21]  = "Crit Rating (Spell)",
    [28]  = "Haste Rating (Melee)",
    [29]  = "Haste Rating (Ranged)",
    [30]  = "Haste Rating (Spell)",
    [31]  = "Hit Rating",
    [32]  = "Crit Rating",
    [35]  = "Resilience Rating",
    [36]  = "Haste Rating",
    [37]  = "Expertise Rating",
    [38]  = "Attack Power",
    [39]  = "Ranged Attack Power",
    [43]  = "Mana per 5s",
    [44]  = "Armor Penetration Rating",
    [45]  = "Spell Power",
    [46]  = "Health per 5s",
    [47]  = "Spell Penetration",
    [48]  = "Block Value",
    [100] = ARMOR or "Armor",
    [101] = "Holy Resistance",
    [102] = "Fire Resistance",
    [103] = "Nature Resistance",
    [104] = "Frost Resistance",
    [105] = "Shadow Resistance",
    [106] = "Arcane Resistance",
}

local MAX_YIELD_ROWS = 10
local MAX_SUBSTANCES = 8

-- Selection state -----------------------------------------------------------

local target = nil       -- { inv = <1-19> }
local donor = nil        -- { bag = <0-4>, slot = <1-based> }
local coins = 0
local subChecked = {}    -- [serverIndex] = true
local substances = {}    -- last S reply: { idx, itemId, red, eligible }
local previewDirty = false
local previewTimer = 0

-- GetCursorInfo gives no source location on 3.3.5, so remember where the
-- last item pickup came from (the standard addon trick).
local lastPickup = nil
hooksecurefunc("PickupContainerItem", function(bag, slot)
    lastPickup = { kind = "bag", bag = bag, slot = slot }
end)
hooksecurefunc("PickupInventoryItem", function(inv)
    lastPickup = { kind = "inv", inv = inv }
end)

-- Window --------------------------------------------------------------------

local frame = CreateFrame("Frame", "InfusionForgeFrame", UIParent)
frame:SetWidth(400)
frame:SetHeight(430)
frame:SetPoint("CENTER")
frame:SetFrameStrata("DIALOG")
-- Border via backdrop; the fill is a SOLID color texture (texture-file
-- backgrounds proved unreliable: the dialog one is translucent and the
-- 3.3.5 client lacks the later opaque ones).
frame:SetBackdrop({
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    edgeSize = 32,
})
local bg = frame:CreateTexture(nil, "BACKGROUND")
bg:SetPoint("TOPLEFT", 9, -9)
bg:SetPoint("BOTTOMRIGHT", -9, 9)
bg:SetTexture(0.09, 0.08, 0.06, 1) -- opaque near-black parchment tone
frame:SetMovable(true)
frame:EnableMouse(true)
frame:RegisterForDrag("LeftButton")
frame:SetScript("OnDragStart", frame.StartMoving)
frame:SetScript("OnDragStop", frame.StopMovingOrSizing)
frame:Hide()
tinsert(UISpecialFrames, "InfusionForgeFrame")

local title = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
title:SetPoint("TOP", 0, -16)
title:SetText("Infusion")

local close = CreateFrame("Button", nil, frame, "UIPanelCloseButton")
close:SetPoint("TOPRIGHT", -6, -6)

local status = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
status:SetPoint("BOTTOM", 0, 18)
status:SetText("")

-- Item wells ----------------------------------------------------------------

local function WellLabel(parent, well, text)
    local label = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    label:SetPoint("BOTTOM", well, "TOP", 0, 4)
    label:SetText(text)
end

local function MakeWell(name, x)
    local well = CreateFrame("Button", name, frame, "ItemButtonTemplate")
    well:SetPoint("TOPLEFT", x, -60)
    return well
end

local targetWell = MakeWell("InfusionForgeTargetWell", 80)
local donorWell = MakeWell("InfusionForgeDonorWell", 280)
WellLabel(frame, targetWell, "Target (equipped)")
WellLabel(frame, donorWell, "Sacrifice")

local arrow = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
arrow:SetPoint("TOPLEFT", 180, -68)
arrow:SetText("<<<")

local function RequestPreview()
    previewDirty = true
    previewTimer = 0.3
end

local function SetWellItem(well, texture, link)
    SetItemButtonTexture(well, texture)
    well.link = link
end

local function RefreshWells()
    if target then
        SetWellItem(targetWell, GetInventoryItemTexture("player", target.inv),
                    GetInventoryItemLink("player", target.inv))
        if not targetWell.link then -- item vanished (destroyed/unequipped)
            target = nil
            SetWellItem(targetWell, nil, nil)
        end
    else
        SetWellItem(targetWell, nil, nil)
    end
    if donor then
        local texture = GetContainerItemInfo(donor.bag, donor.slot)
        SetWellItem(donorWell, texture, GetContainerItemLink(donor.bag, donor.slot))
        if not donorWell.link then
            donor = nil
            SetWellItem(donorWell, nil, nil)
        end
    else
        SetWellItem(donorWell, nil, nil)
    end
end

local function TakeCursor(well)
    if not CursorHasItem() or not lastPickup then
        return
    end
    if well == targetWell and lastPickup.kind == "inv"
       and lastPickup.inv >= 1 and lastPickup.inv <= 19 then
        target = { inv = lastPickup.inv }
    elseif well == donorWell and lastPickup.kind == "bag"
       and lastPickup.bag >= 0 and lastPickup.bag <= 4 then
        donor = { bag = lastPickup.bag, slot = lastPickup.slot }
    else
        status:SetText(well == targetWell
            and "Target must come from your equipped gear."
            or "Sacrifice must come from your bags.")
        return
    end
    ClearCursor()
    coins = 0
    wipe(subChecked)
    RefreshWells()
    RequestPreview()
end

for _, well in ipairs({ targetWell, donorWell }) do
    well:RegisterForClicks("LeftButtonUp", "RightButtonUp")
    well:SetScript("OnReceiveDrag", function(self) TakeCursor(self) end)
    well:SetScript("OnClick", function(self, button)
        if CursorHasItem() then
            TakeCursor(self)
        elseif button == "RightButton" then
            if self == targetWell then target = nil else donor = nil end
            RefreshWells()
            RequestPreview()
        end
    end)
    well:SetScript("OnEnter", function(self)
        if self.link then
            GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
            GameTooltip:SetHyperlink(self.link)
            GameTooltip:Show()
        end
    end)
    well:SetScript("OnLeave", function() GameTooltip:Hide() end)
end

-- Yield list ----------------------------------------------------------------

local yieldHeader = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
yieldHeader:SetPoint("TOPLEFT", 24, -120)
yieldHeader:SetText("Transferred essence")

local yieldRows = {}
for i = 1, MAX_YIELD_ROWS do
    local row = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    row:SetPoint("TOPLEFT", 32, -120 - 16 * i)
    row:SetText("")
    yieldRows[i] = row
end

-- Risk bar ------------------------------------------------------------------

local riskBar = CreateFrame("StatusBar", nil, frame)
riskBar:SetPoint("TOPLEFT", 190, -140)
riskBar:SetWidth(140)
riskBar:SetHeight(18)
riskBar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
riskBar:SetMinMaxValues(0, 100)
riskBar:SetValue(0)
local riskBg = riskBar:CreateTexture(nil, "BACKGROUND")
riskBg:SetAllPoints()
riskBg:SetTexture(0, 0, 0, 0.5)
local riskText = riskBar:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
riskText:SetPoint("CENTER")
riskText:SetText("")
local riskLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
riskLabel:SetPoint("BOTTOMLEFT", riskBar, "TOPLEFT", 0, 4)
riskLabel:SetText("Destruction risk")
local masteryText = frame:CreateFontString(nil, "OVERLAY", "GameFontRedSmall")
masteryText:SetPoint("TOPLEFT", riskBar, "BOTTOMLEFT", 0, -2)
masteryText:SetWidth(180)
masteryText:SetJustifyH("LEFT")
masteryText:SetText("")

local function PaintRisk(pct, penaltyPct)
    riskBar:SetValue(pct)
    riskText:SetText(pct .. "%")
    if pct < 15 then riskBar:SetStatusBarColor(0.1, 0.8, 0.1)
    elseif pct < 40 then riskBar:SetStatusBarColor(0.9, 0.8, 0.1)
    elseif pct < 60 then riskBar:SetStatusBarColor(0.9, 0.5, 0.1)
    else riskBar:SetStatusBarColor(0.9, 0.1, 0.1) end
    masteryText:SetText(penaltyPct > 0
        and ("includes +" .. penaltyPct .. "% beyond your mastery") or "")
end

-- Mitigation ----------------------------------------------------------------

local coinLabel = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
coinLabel:SetPoint("TOPLEFT", 190, -200)
coinLabel:SetText("Coins pledged: 0")

local function CoinButton(text, dx, delta)
    local btn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
    btn:SetWidth(22)
    btn:SetHeight(18)
    btn:SetPoint("TOPLEFT", 190 + dx, -214)
    btn:SetText(text)
    btn:SetScript("OnClick", function()
        coins = math.max(0, math.min(coins + delta, 40))
        coinLabel:SetText("Coins pledged: " .. coins)
        RequestPreview()
    end)
    return btn
end
CoinButton("-", 0, -1)
CoinButton("+", 26, 1)

local subChecks = {}
for i = 1, MAX_SUBSTANCES do
    local check = CreateFrame("CheckButton", "InfusionForgeSub" .. i, frame,
                              "UICheckButtonTemplate")
    check:SetWidth(20)
    check:SetHeight(20)
    check:SetPoint("TOPLEFT", 186, -236 - 21 * (i - 1))
    check.label = frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    check.label:SetPoint("LEFT", check, "RIGHT", 2, 0)
    check:SetScript("OnClick", function(self)
        if self.serverIndex then
            subChecked[self.serverIndex] = self:GetChecked() and true or nil
            RequestPreview()
        end
    end)
    check:Hide()
    subChecks[i] = check
end

local function PaintSubstances()
    for i, check in ipairs(subChecks) do
        local sub = substances[i]
        if sub then
            check.serverIndex = sub.idx
            local name = GetItemInfo(sub.itemId) or ("Item " .. sub.itemId)
            if sub.eligible then
                check:Enable()
                check.label:SetText(name .. " (-" .. sub.red .. "%)")
                check.label:SetTextColor(0.9, 0.9, 0.9)
                check:SetChecked(subChecked[sub.idx] and true or false)
            else
                check:Disable()
                check:SetChecked(false)
                subChecked[sub.idx] = nil
                check.label:SetText(name .. " - too weak")
                check.label:SetTextColor(0.5, 0.5, 0.5)
            end
            check:Show()
            check.label:Show()
        else
            check.serverIndex = nil
            check:Hide()
            check.label:SetText("")
        end
    end
end

-- Infuse --------------------------------------------------------------------

local function SubMask()
    local mask = 0
    for idx in pairs(subChecked) do
        mask = mask + 2 ^ idx
    end
    return mask
end

local function SendMsg(body)
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

local function Args()
    return target.inv .. "\t" .. donor.bag .. "\t" .. donor.slot ..
           "\t" .. coins .. "\t" .. SubMask()
end

local lastRisk = 0
local yieldCount = 0

-- StaticPopup text supports at most TWO %s substitutions.
StaticPopupDialogs["INFUSIONFORGE_CONFIRM"] = {
    text = "Sacrifice %s?\n\nDestruction risk: %s - failure DESTROYS the target item!",
    button1 = YES,
    button2 = NO,
    OnAccept = function()
        if target and donor then
            SendMsg("X\t" .. Args())
        end
    end,
    timeout = 0,
    whileDead = 1,
    hideOnEscape = 1,
}

local infuseBtn = CreateFrame("Button", nil, frame, "UIPanelButtonTemplate")
infuseBtn:SetWidth(120)
infuseBtn:SetHeight(24)
infuseBtn:SetPoint("BOTTOM", 0, 40)
infuseBtn:SetText("Infuse")
infuseBtn:Disable()
infuseBtn:SetScript("OnClick", function()
    if target and donor then
        StaticPopup_Show("INFUSIONFORGE_CONFIRM",
                         donorWell.link or "?", lastRisk .. "%")
    end
end)

-- Preview & events ----------------------------------------------------------

local function ClearPreview(text)
    for _, row in ipairs(yieldRows) do
        row:SetText("")
    end
    yieldCount = 0
    substances = {}
    PaintSubstances()
    PaintRisk(0, 0)
    riskText:SetText("")
    infuseBtn:Disable()
    status:SetText(text or "")
end

local REFUSALS = {
    OFF = "Infusions are disabled.",
    LEVEL = "You are too low level to infuse.",
    NOITEM = "Place a target and a sacrifice.",
    BASIC = "That item is too basic to hold an infusion.",
    NOYIELD = "That sacrifice has no essence worth transferring.",
}

frame:SetScript("OnUpdate", function(_, elapsed)
    if previewDirty then
        previewTimer = previewTimer - elapsed
        if previewTimer <= 0 then
            previewDirty = false
            if target and donor then
                SendMsg("P\t" .. Args())
            else
                ClearPreview(status:GetText()) -- keep any result message
            end
        end
    end
end)

local events = CreateFrame("Frame")
events:RegisterEvent("CHAT_MSG_ADDON")
events:RegisterEvent("BAG_UPDATE")
events:SetScript("OnEvent", function(_, event, prefix, message, _, sender)
    if event == "BAG_UPDATE" then
        if frame:IsShown() then
            RefreshWells()
        end
        return
    end
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end

    if message == "O" then
        frame:Show()
        RefreshWells()
        RequestPreview()
        return
    end

    local kind, a, rest = string.match(message, "^([^\t]+)\t([^\t]+)\t?(.*)$")
    if kind == "R" and a == "H" then
        local risk, base, penalty = strsplit("\t", rest)
        lastRisk = tonumber(risk) or 0
        PaintRisk(lastRisk, tonumber(penalty) or 0)
        for _, row in ipairs(yieldRows) do
            row:SetText("")
        end
        yieldCount = 0
        substances = {}
        infuseBtn:Enable()
        status:SetText("")
    elseif kind == "R" and a == "Y" then
        -- Y may span several messages; append after the rows already
        -- filled (counted explicitly: GetText() is nil when empty).
        for entry in string.gmatch(rest or "", "[^;]+") do
            local prop, amount = string.match(entry, "^(%d+):(-?%d+)$")
            if prop and yieldRows[yieldCount + 1] then
                yieldCount = yieldCount + 1
                local name = PROPERTY_NAMES[tonumber(prop)] or ("Property " .. prop)
                yieldRows[yieldCount]:SetText("+" .. amount .. " " .. name)
                yieldRows[yieldCount]:SetTextColor(0.2, 1.0, 0.2)
            end
        end
    elseif kind == "R" and a == "S" then
        for entry in string.gmatch(rest or "", "[^;]+") do
            local idx, itemId, red, elig =
                string.match(entry, "^(%d+):(%d+):(%d+):([01])$")
            if idx then
                table.insert(substances, {
                    idx = tonumber(idx), itemId = tonumber(itemId),
                    red = tonumber(red), eligible = elig == "1",
                })
            end
        end
        PaintSubstances()
    elseif kind == "R" and a == "E" then
        ClearPreview(REFUSALS[rest] or ("Cannot infuse (" .. (rest or "?") .. ")"))
    elseif kind == "X" then
        if a == "OK" then
            donor = nil
            coins = 0
            wipe(subChecked)
            coinLabel:SetText("Coins pledged: 0")
            RefreshWells()
            RequestPreview()
            status:SetText("The infusion holds!")
            PlaySound("igQuestListComplete")
        elseif a == "DEAD" then
            target = nil
            donor = nil
            RefreshWells()
            ClearPreview("The infusion destabilized - the item is DESTROYED.")
            PlaySound("igQuestFailed")
        else
            status:SetText("The infusion was refused - see chat.")
        end
    end
end)

SLASH_INFUSIONFORGE1 = "/infusion"
SlashCmdList["INFUSIONFORGE"] = function()
    if frame:IsShown() then
        frame:Hide()
    else
        frame:Show()
        RefreshWells()
        RequestPreview()
    end
end
