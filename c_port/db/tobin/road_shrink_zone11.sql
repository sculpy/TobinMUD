-- Road-shrink pilot: First Ring of Roads (zone 11), 2026-08-22.
-- Thins pure single-file corridor stretches by roughly half while
-- keeping every cross-zone boundary room at its existing vnum. Also
-- fixes a pre-existing data bug: rooms 670/675 were live, connected,
-- reachable rooms in this zone's own vnum range but had zone=NULL.
-- Every statement here is naturally idempotent (DELETEs of rows that
-- no longer exist, and UPDATEs setting a value to what it already is,
-- are no-ops), matching this project's re-apply-safe db/tobin/ convention.

UPDATE room SET zone = 11 WHERE vnum IN (670, 675);

UPDATE roomexit SET destination = 653 WHERE vnum = 651 AND direction = 9;
UPDATE roomexit SET destination = 651 WHERE vnum = 653 AND direction = 6;
UPDATE roomexit SET destination = 657 WHERE vnum = 653 AND direction = 9;
UPDATE roomexit SET destination = 653 WHERE vnum = 657 AND direction = 1;

UPDATE roomexit SET destination = 659 WHERE vnum = 657 AND direction = 3;
UPDATE roomexit SET destination = 657 WHERE vnum = 659 AND direction = 1;

UPDATE roomexit SET destination = 663 WHERE vnum = 661 AND direction = 2;
UPDATE roomexit SET destination = 661 WHERE vnum = 663 AND direction = 0;
UPDATE roomexit SET destination = 666 WHERE vnum = 663 AND direction = 2;
UPDATE roomexit SET destination = 663 WHERE vnum = 666 AND direction = 1;

UPDATE roomexit SET destination = 668 WHERE vnum = 660 AND direction = 3;
UPDATE roomexit SET destination = 660 WHERE vnum = 668 AND direction = 1;
UPDATE roomexit SET destination = 670 WHERE vnum = 668 AND direction = 7;
UPDATE roomexit SET destination = 668 WHERE vnum = 670 AND direction = 8;
UPDATE roomexit SET destination = 674 WHERE vnum = 670 AND direction = 7;
UPDATE roomexit SET destination = 670 WHERE vnum = 674 AND direction = 1;

UPDATE roomexit SET destination = 685 WHERE vnum = 683 AND direction = 6;
UPDATE roomexit SET destination = 683 WHERE vnum = 685 AND direction = 9;
UPDATE roomexit SET destination = 688 WHERE vnum = 685 AND direction = 6;
UPDATE roomexit SET destination = 685 WHERE vnum = 688 AND direction = 3;

UPDATE roomexit SET destination = 678 WHERE vnum = 676 AND direction = 8;
UPDATE roomexit SET destination = 676 WHERE vnum = 678 AND direction = 7;
UPDATE roomexit SET destination = 682 WHERE vnum = 678 AND direction = 8;
UPDATE roomexit SET destination = 678 WHERE vnum = 682 AND direction = 3;

UPDATE roomexit SET destination = 690 WHERE vnum = 682 AND direction = 2;
UPDATE roomexit SET destination = 682 WHERE vnum = 690 AND direction = 0;

UPDATE roomexit SET destination = 694 WHERE vnum = 692 AND direction = 2;
UPDATE roomexit SET destination = 692 WHERE vnum = 694 AND direction = 6;
UPDATE roomexit SET destination = 698 WHERE vnum = 694 AND direction = 9;
UPDATE roomexit SET destination = 694 WHERE vnum = 698 AND direction = 6;

DELETE FROM roomexit WHERE vnum IN (652,654,655,656,658,662,664,665,667,669,671,672,673,677,679,680,681,684,686,687,689,693,695,696,697);
DELETE FROM room WHERE vnum IN (652,654,655,656,658,662,664,665,667,669,671,672,673,677,679,680,681,684,686,687,689,693,695,696,697);
