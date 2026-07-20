-- PropertyOverlay: displays per-item-instance stat overrides served by the
-- mod-property-override server module.
--
-- Protocol (see PropertyOverrideAddonMsg.h):
--   send    IPROP\tQ\tE\t<invSlot>        (equipped, client 1-based slot id)
--           IPROP\tQ\tB\t<bag>\t<slot>    (bag 0-4, slot 1-based)
--   receive IPROP\tR\t<coords...>\t<prop>:<value>:<expiry>;...  or  ...\t-
--           IPROP\tI                      (invalidate: flush cache)
-- Transport: addon whisper to self; the server swallows the outgoing
-- whisper and replies on the same channel.

local PREFIX = "IPROP"

-- Ids are the game's ITEM_MOD_* values (>=100 are module-custom).
-- SPELL_STAT*_NAME are the client's own localized primary stat names.
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

local cache = {}    -- key -> rows table, or false for "none"
local pending = {}  -- key -> GetTime() of last request (throttle)
local currentKey    -- key of the item the visible tooltip is showing

local REQUEST_COOLDOWN = 2 -- seconds between requests for the same key

local function ParseRows(payload)
    if payload == "-" or payload == "" or payload == nil then
        return false
    end
    local rows = {}
    for entry in string.gmatch(payload, "[^;]+") do
        local prop, value = string.match(entry, "^(%d+):(-?%d+):%d+$")
        if prop then
            table.insert(rows, { property = tonumber(prop), value = tonumber(value) })
        end
    end
    if #rows == 0 then
        return false
    end
    return rows
end

local function Annotate(rows)
    if not rows then
        return
    end
    for _, row in ipairs(rows) do
        local name = PROPERTY_NAMES[row.property] or ("Property " .. row.property)
        local sign = row.value >= 0 and "+" or ""
        GameTooltip:AddLine("Upgrade: " .. sign .. row.value .. " " .. name, 0.2, 1.0, 0.2)
    end
    GameTooltip:Show()
end

local function Request(key, body)
    currentKey = key
    if cache[key] ~= nil then
        Annotate(cache[key])
        return
    end
    local now = GetTime()
    if pending[key] and now - pending[key] < REQUEST_COOLDOWN then
        return
    end
    pending[key] = now
    SendAddonMessage(PREFIX, body, "WHISPER", UnitName("player"))
end

hooksecurefunc(GameTooltip, "SetInventoryItem", function(_, unit, slot)
    if unit == "player" and slot and slot >= 1 and slot <= 19 then
        Request("E:" .. slot, "Q\tE\t" .. slot)
    end
end)

hooksecurefunc(GameTooltip, "SetBagItem", function(_, bag, slot)
    if bag and slot and bag >= 0 and bag <= 4 then
        Request("B:" .. bag .. ":" .. slot, "Q\tB\t" .. bag .. "\t" .. slot)
    end
end)

GameTooltip:HookScript("OnHide", function()
    currentKey = nil
end)

local frame = CreateFrame("Frame")
frame:RegisterEvent("CHAT_MSG_ADDON")
frame:SetScript("OnEvent", function(_, _, prefix, message, _, sender)
    if prefix ~= PREFIX or sender ~= UnitName("player") then
        return
    end

    if message == "I" then
        wipe(cache)
        wipe(pending)
        return
    end

    -- Reply: R\tE\t<slot>\t<rows>  or  R\tB\t<bag>\t<slot>\t<rows>
    local kind, a, b, c = strsplit("\t", message)
    if kind ~= "R" then
        return
    end

    local key, payload
    if a == "E" then
        key, payload = "E:" .. (b or "?"), c
    elseif a == "B" then
        key, payload = "B:" .. (b or "?") .. ":" .. (c or "?"), select(5, strsplit("\t", message))
    else
        return
    end

    cache[key] = ParseRows(payload)
    pending[key] = nil

    -- If the tooltip is still on this item, repaint it live.
    if currentKey == key and GameTooltip:IsShown() and cache[key] then
        Annotate(cache[key])
    end
end)
