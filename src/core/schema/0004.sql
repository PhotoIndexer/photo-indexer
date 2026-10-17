-- Canon com "Girar auto" desligado: a tag Orientation diz "normal" e a
-- posição real fica no MakerNote (imaging::makerNoteOrientation). As JPEG
-- Canon já indexadas com orientação 1 voltam para a fila do Indexer: a
-- miniatura sai na posição certa; o picture_hash não muda (é o da tag), e a
-- IA e as faces só refazem as que mudaram de posição (Catalog::applyIndex).
UPDATE photo SET state = 'pending' WHERE state = 'indexed' AND make = 'Canon' AND orientation = 1 AND is_raw = 0;
