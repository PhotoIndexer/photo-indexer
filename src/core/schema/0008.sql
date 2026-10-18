-- "Ao longo dos anos" (tela Pessoas): a foto escolhida para cada período de
-- uma pessoa, em cada frequência (mês, trimestre, semestre, ano). A primeira
-- escolha é automática ('auto'); a pessoa pode trocar ('user') ou tirar o
-- período da montagem (hidden). Fica guardada para voltar depois.
CREATE TABLE person_timeline (
    person_id TEXT NOT NULL REFERENCES person(id) ON DELETE CASCADE,
    frequency TEXT NOT NULL,
    period TEXT NOT NULL,
    face_id TEXT REFERENCES face(id) ON DELETE SET NULL,
    chosen_by TEXT NOT NULL DEFAULT 'auto',
    hidden INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (person_id, frequency, period)
);
