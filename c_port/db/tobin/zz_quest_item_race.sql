-- Per-race quest-item table demo content (Sneezy -> Tobin feature audit --
-- disclosed NOT a port, see tobin_migrations.sql's `quest_item` doc
-- comment). A single sample quest ("heirloom", stage 1) so `questitem`/
-- `quest claim` have real data to exercise -- reuses each race's own
-- starting racial weapon vnum (zz_newbie_gear_race.sql) as its "heirloom"
-- reward, so no new object authoring was needed to demonstrate the
-- system end to end. An immortal is expected to author real quest
-- content and rewards with `questdef`/`questitem` on top of this.
-- Race values match player_race_t (being.h): HUMAN=0, ELF=1, OGRE=2,
-- DWARF=3, HOBBIT=4, GNOME=5.
INSERT INTO `quest_def` (quest_name, stage, description) VALUES
('heirloom', 1, 'You have proven yourself worthy of your family''s heirloom weapon. Type `quest claim heirloom` to receive it.')
ON DUPLICATE KEY UPDATE `description` = VALUES(`description`);
INSERT INTO `quest_item` (quest_name, stage, race, obj_vnum) VALUES
('heirloom', 1, 0, 36996), -- human: sword
('heirloom', 1, 1, 36998), -- elf: longknife
('heirloom', 1, 2, 37001), -- ogre: club
('heirloom', 1, 3, 36997), -- dwarf: hand axe
('heirloom', 1, 4, 37000), -- hobbit: walking stick
('heirloom', 1, 5, 36999)  -- gnome: dagger
ON DUPLICATE KEY UPDATE `obj_vnum` = VALUES(`obj_vnum`);
