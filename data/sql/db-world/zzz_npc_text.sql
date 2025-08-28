-- npc text
SET @npc_text_3v3_soloq := " ";
DELETE FROM `npc_text` WHERE `ID` = 60015;
INSERT INTO `npc_text` (`id`, `text0_0`, `text0_1`, `Probability0`) VALUES (60015, @npc_text_3v3_soloq, @npc_text_3v3_soloq, 1);