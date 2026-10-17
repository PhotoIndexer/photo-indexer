-- Fotos privadas: o original vira <nome>.photoidxe (AES-256-GCM) no mesmo
-- lugar e a miniatura vira <id>.pidx; só aparecem no álbum "Private" com a
-- senha (src/core/CatalogPrivate.cpp).
--
-- ``private``         1 = privada: some de todas as árvores e da grade, menos
--                     do álbum privado (com a senha)
-- ``private_state``   fila da etapa "privacy": encrypt | decrypt | done | error
--                     ('' = pública); a troca de chave refaz as de key_id antigo
-- ``key_id``          id público da chave que cifrou (Crypto.h)
-- ``file_sha256``     SHA-256 do arquivo original (antes de cifrar)
-- ``encrypted_sha256`` SHA-256 do .photoidxe (depois de cifrar)
-- ``original_name``   nome do arquivo antes de cifrar (volta com ele)
ALTER TABLE photo ADD COLUMN private INTEGER NOT NULL DEFAULT 0;
ALTER TABLE photo ADD COLUMN private_state TEXT NOT NULL DEFAULT '';
ALTER TABLE photo ADD COLUMN key_id TEXT NOT NULL DEFAULT '';
ALTER TABLE photo ADD COLUMN file_sha256 TEXT NOT NULL DEFAULT '';
ALTER TABLE photo ADD COLUMN encrypted_sha256 TEXT NOT NULL DEFAULT '';
ALTER TABLE photo ADD COLUMN original_name TEXT NOT NULL DEFAULT '';
CREATE INDEX photo_private ON photo(private, private_state);

-- O álbum "Private" (raiz, criado pelo app) e os sub-álbuns dele.
ALTER TABLE album ADD COLUMN private INTEGER NOT NULL DEFAULT 0;
