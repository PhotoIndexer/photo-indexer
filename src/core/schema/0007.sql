-- Grupos de pessoas (painel lateral, aba Pessoas): a pessoa cria os seus
-- ("Família", "Time de basquete") e põe pessoas neles; quem não tem grupo
-- aparece em "Com nome" ou "Sem nome". Apagar o grupo solta as pessoas.
CREATE TABLE person_group (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    position INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL
);
ALTER TABLE person ADD COLUMN group_id TEXT REFERENCES person_group(id) ON DELETE SET NULL;
CREATE INDEX person_group_id ON person(group_id);
