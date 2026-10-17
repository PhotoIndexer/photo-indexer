-- Filas das etapas por índices parciais: o Pipeline pede os próximos lotes e
-- as contagens o tempo todo, na thread principal. Com 60 mil fotos
-- pendentes, os índices de uma coluna (state, ai_state, faces_state) davam
-- varredura de quase a tabela inteira (~40 ms por pedido); um índice que só
-- tem a fila responde em < 1 ms.
DROP INDEX IF EXISTS photo_state;
DROP INDEX IF EXISTS photo_ai_state;
DROP INDEX IF EXISTS photo_faces_state;
CREATE INDEX photo_queue_index ON photo(added_at, folder_id, private) WHERE state = 'pending';
CREATE INDEX photo_queue_ai ON photo(indexed_at) WHERE state = 'indexed' AND ai_state = 'pending' AND private = 0 AND thumb_path != '';
CREATE INDEX photo_queue_faces ON photo(indexed_at, folder_id) WHERE state = 'indexed' AND faces_state = 'pending' AND private = 0 AND thumb_path != '';

-- O índice de privadas só com as privadas (as públicas são quase todas: um
-- índice com elas fazia a grade inteira ir pelo índice e ordenar à parte).
DROP INDEX IF EXISTS photo_private;
CREATE INDEX photo_private ON photo(private_state) WHERE private = 1;
