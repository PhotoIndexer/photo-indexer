import json, sqlite3, uuid, collections, datetime
c = sqlite3.connect('/cache/tut-data/data.sqlite3')
c.execute('PRAGMA foreign_keys = ON')
q = lambda s, a=(): c.execute(s, a).fetchall()
now = datetime.datetime.utcnow().strftime('%Y-%m-%dT%H:%M:%S.000Z')
man = json.load(open('/m/manifest.json'))
NAMES = {1: 'Ana Souza', 2: 'Lia Souza', 3: 'Bruno Lima', 4: 'Diego Prado', 5: 'Helena Souza', 6: 'Theo Souza'}
# Quem é cada pessoa: voto pelas fotos de uma pessoa só.
votes = collections.defaultdict(collections.Counter)
for pid, path in q("SELECT f.person_id, p.path FROM face f JOIN photo p ON p.id = f.photo_id WHERE f.person_id IS NOT NULL"):
    rel = path.split('/tut/Fotos/', 1)[-1]
    whos = man.get(rel, [])
    if len(whos) == 1:
        votes[pid][whos[0]] += 1
main = {}
for pid, cnt in sorted(votes.items(), key=lambda kv: -sum(kv[1].values())):
    who = cnt.most_common(1)[0][0]
    if who in main:   # mesma pessoa partida em duas: junta
        q("UPDATE face SET person_id = ? WHERE person_id = ?", (main[who], pid))
        q("DELETE FROM person WHERE id = ?", (pid,))
    else:
        main[who] = pid
for who, pid in main.items():
    if who in NAMES:
        q("UPDATE person SET name = ?, updated_at = ? WHERE id = ?", (NAMES[who], now, pid))
q("""UPDATE person SET face_count = (SELECT count(*) FROM face f WHERE f.person_id = person.id),
     photo_count = (SELECT count(DISTINCT photo_id) FROM face f WHERE f.person_id = person.id),
     cover_face_id = (SELECT id FROM face f WHERE f.person_id = person.id ORDER BY score DESC LIMIT 1)""")
q("DELETE FROM person WHERE face_count = 0")
# Grupos.
def group(name, pos, whos):
    gid = str(uuid.uuid4())
    q("INSERT INTO person_group(id, name, position, created_at) VALUES(?, ?, ?, ?)", (gid, name, pos, now))
    for w in whos:
        if w in main:
            q("UPDATE person SET group_id = ? WHERE id = ?", (gid, main[w]))
group('Família Souza', 1, [1, 2, 5, 6])
group('Amigos', 2, [3, 4])
q("INSERT OR REPLACE INTO setting(key, value) VALUES('people/minPhotos', '3')")
# Álbuns.
cols = [r[1] for r in q("PRAGMA table_info(album_photo)")]
def album(name, pos, where):
    aid = str(uuid.uuid4())
    q("INSERT INTO album(id, parent_id, name, position, created_at, private) VALUES(?, NULL, ?, ?, ?, 0)", (aid, name, pos, now))
    for (pid,) in q("SELECT id FROM photo WHERE " + where):
        if 'added_at' in cols:
            q("INSERT INTO album_photo(album_id, photo_id, added_at) VALUES(?, ?, ?)", (aid, pid, now))
        else:
            q("INSERT INTO album_photo(album_id, photo_id) VALUES(?, ?)", (aid, pid))
album('Férias na praia', 1, "dir LIKE '%/Viagem-Praia'")
album('Melhores de 2024', 2, "(dir LIKE '%/Pets' OR dir LIKE '%/Trilha') AND rowid % 2 = 0")
c.commit()
print(q("SELECT name, photo_count, coalesce(g.name, '-') FROM person p LEFT JOIN person_group g ON g.id = p.group_id ORDER BY photo_count DESC"))
print(q("SELECT a.name, count(*) FROM album a JOIN album_photo l ON l.album_id = a.id GROUP BY 1"))
