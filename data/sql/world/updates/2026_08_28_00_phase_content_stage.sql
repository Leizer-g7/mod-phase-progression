ALTER TABLE `phase_progression_state`
    ADD COLUMN `active_content_stage` TINYINT UNSIGNED NOT NULL DEFAULT 0
    AFTER `active_phase`;
