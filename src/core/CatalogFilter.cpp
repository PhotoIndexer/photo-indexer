// SPDX-License-Identifier: BSD-2-Clause
#include "core/Catalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>

// Filtro personalizado da grade: regras {campo, operador, valor} encadeadas
// com E (todas) ou OU (qualquer). Campos e operadores são chaves de uma lista
// FECHADA (kFields): a interface nunca manda SQL; os valores vão sempre como
// parâmetros ligados.

namespace pix {

namespace {

struct Field
{
    const char *key;
    const char *type;   ///< text | number | date | bool | enum | category | album | person | exif
    const char *group;  ///< agrupamento na interface
    const char *sql;    ///< expressão sobre a foto ``p`` (vazio nos de relação)
    const char *choices = nullptr;  ///< enum: valores separados por '|'
};

const Field kFields[] = {
    // Arquivo
    {"name", "text", "file", "p.name"},
    {"folder", "text", "file", "p.dir"},
    {"format", "text", "file", "p.format"},
    {"raw", "bool", "file", "p.is_raw"},
    {"sizeMb", "number", "file", "(p.size / 1048576.0)"},
    {"fileModified", "date", "file", "substr(p.file_modified_at, 1, 10)"},
    {"added", "date", "file", "substr(p.added_at, 1, 10)"},
    {"disk", "text", "file", "(SELECT d.label FROM folder f JOIN disk d ON d.id = f.disk_id WHERE f.id = p.folder_id)"},
    {"diskKind", "enum", "file", "(SELECT d.kind FROM folder f JOIN disk d ON d.id = f.disk_id WHERE f.id = p.folder_id)",
     "local|removable|network"},
    {"pictureHash", "text", "file", "p.picture_hash"},
    // Imagem
    {"width", "number", "image", "p.width"},
    {"height", "number", "image", "p.height"},
    {"megapixels", "number", "image", "(p.width * p.height / 1000000.0)"},
    {"shape", "enum", "image",
     "(CASE WHEN p.width = 0 OR p.height = 0 THEN '' WHEN abs(p.width - p.height) <= 0.02 * p.width THEN 'square' "
     "WHEN p.width > p.height THEN 'landscape' ELSE 'portrait' END)",
     "landscape|portrait|square"},
    {"sharpness", "number", "image", "p.sharpness"},
    {"faces", "number", "image", "p.face_count"},
    // Datas
    {"taken", "date", "date", "substr(p.taken_at, 1, 10)"},
    {"year", "number", "date", "p.year"},
    {"month", "number", "date", "p.month"},
    {"dateSource", "enum", "date", "p.taken_source", "exif|file"},
    {"modified", "date", "date", "substr(p.modified_at, 1, 10)"},
    // Câmera
    {"camera", "text", "camera", "p.camera"},
    {"make", "text", "camera", "p.make"},
    {"model", "text", "camera", "p.model"},
    {"lens", "text", "camera", "p.lens"},
    {"iso", "number", "camera", "p.iso"},
    {"aperture", "number", "camera", "p.aperture"},
    {"shutter", "number", "camera", "p.shutter"},
    {"focalLength", "number", "camera", "p.focal_length"},
    {"focalLength35", "number", "camera", "p.focal_length_35"},
    {"exposureBias", "number", "camera", "p.exposure_bias"},
    {"flashFired", "bool", "camera", "(coalesce(p.flash, 0) % 2)"},
    {"software", "text", "camera", "p.software"},
    {"artist", "text", "camera", "p.artist"},
    {"copyright", "text", "camera", "p.copyright"},
    {"description", "text", "camera", "p.description"},
    // Local
    {"hasGps", "bool", "location", "(p.latitude IS NOT NULL)"},
    {"country", "text", "location", "(SELECT pl.country FROM place pl WHERE pl.id = p.place_id)"},
    {"state", "text", "location", "(SELECT pl.state FROM place pl WHERE pl.id = p.place_id)"},
    {"city", "text", "location", "(SELECT pl.city FROM place pl WHERE pl.id = p.place_id)"},
    {"latitude", "number", "location", "p.latitude"},
    {"longitude", "number", "location", "p.longitude"},
    {"altitude", "number", "location", "p.altitude"},
    // Organização
    {"category", "category", "organize", ""},
    {"album", "album", "organize", ""},
    {"person", "person", "organize", ""},
    // EXIF: qualquer tag pelo nome
    {"exif", "exif", "exif", ""},
};

const QHash<QString, QStringList> &opsByType()
{
    static const QHash<QString, QStringList> ops{
        {QStringLiteral("text"), {QStringLiteral("contains"), QStringLiteral("notContains"), QStringLiteral("eq"),
                                  QStringLiteral("ne"), QStringLiteral("startsWith"), QStringLiteral("endsWith"),
                                  QStringLiteral("empty"), QStringLiteral("notEmpty")}},
        {QStringLiteral("number"), {QStringLiteral("eq"), QStringLiteral("ne"), QStringLiteral("gt"), QStringLiteral("gte"),
                                    QStringLiteral("lt"), QStringLiteral("lte"), QStringLiteral("between"),
                                    QStringLiteral("empty"), QStringLiteral("notEmpty")}},
        {QStringLiteral("date"), {QStringLiteral("eq"), QStringLiteral("before"), QStringLiteral("after"),
                                  QStringLiteral("between"), QStringLiteral("empty"), QStringLiteral("notEmpty")}},
        {QStringLiteral("bool"), {QStringLiteral("isTrue"), QStringLiteral("isFalse")}},
        {QStringLiteral("enum"), {QStringLiteral("eq"), QStringLiteral("ne")}},
        {QStringLiteral("category"), {QStringLiteral("has"), QStringLiteral("hasNot"), QStringLiteral("any"), QStringLiteral("none")}},
        {QStringLiteral("album"), {QStringLiteral("has"), QStringLiteral("hasNot"), QStringLiteral("any"), QStringLiteral("none")}},
        {QStringLiteral("person"), {QStringLiteral("has"), QStringLiteral("hasNot"), QStringLiteral("any"), QStringLiteral("none")}},
        {QStringLiteral("exif"), {QStringLiteral("exists"), QStringLiteral("notExists"), QStringLiteral("contains"),
                                  QStringLiteral("eq")}},
    };
    return ops;
}

const Field *findField(const QString &key)
{
    for (const Field &f : kFields)
        if (key == QLatin1String(f.key))
            return &f;
    return nullptr;
}

QString escapeLike(const QString &text)
{
    QString out = text;
    out.replace(u'\\', QStringLiteral("\\\\")).replace(u'%', QStringLiteral("\\%")).replace(u'_', QStringLiteral("\\_"));
    return out;
}

/// Condição de uma regra; vazia se a regra não faz sentido (campo/operador
/// desconhecido ou valor faltando) — regra inválida não filtra nada.
QString ruleCondition(const FilterRule &rule, QVariantList &params)
{
    const Field *field = findField(rule.field);
    if (!field || !opsByType().value(QString::fromLatin1(field->type)).contains(rule.op))
        return {};
    const QString type = QString::fromLatin1(field->type);
    const QString expr = QString::fromLatin1(field->sql);
    const QString op = rule.op;
    const QString text = rule.value.toString();

    if (type == QLatin1String("category") || type == QLatin1String("album") || type == QLatin1String("person")) {
        QString sub;
        if (type == QLatin1String("category"))
            sub = op == QLatin1String("any") || op == QLatin1String("none")
                ? QStringLiteral("SELECT photo_id FROM photo_category WHERE source IN ('ai', 'user')")
                : QStringLiteral("WITH RECURSIVE sub(id) AS (SELECT ? UNION ALL SELECT c.id FROM category c JOIN sub "
                                 "ON c.parent_id = sub.id) SELECT photo_id FROM photo_category WHERE source != 'rejected' "
                                 "AND category_id IN (SELECT id FROM sub)");
        else if (type == QLatin1String("album"))
            sub = op == QLatin1String("any") || op == QLatin1String("none")
                ? QStringLiteral("SELECT photo_id FROM album_photo")
                : QStringLiteral("WITH RECURSIVE sub(id) AS (SELECT ? UNION ALL SELECT a.id FROM album a JOIN sub "
                                 "ON a.parent_id = sub.id) SELECT photo_id FROM album_photo WHERE album_id IN (SELECT id FROM sub)");
        else
            sub = op == QLatin1String("any") || op == QLatin1String("none")
                ? QStringLiteral("SELECT photo_id FROM face WHERE person_id IS NOT NULL")
                : QStringLiteral("SELECT photo_id FROM face WHERE person_id = ?");
        if (op == QLatin1String("has") || op == QLatin1String("hasNot")) {
            if (text.isEmpty())
                return {};
            params << text;
        }
        const bool negate = op == QLatin1String("hasNot") || op == QLatin1String("none");
        return QStringLiteral("p.id %1 IN (%2)").arg(negate ? QStringLiteral("NOT") : QString(), sub);
    }

    if (type == QLatin1String("exif")) {
        // value = nome da tag ("LensModel", "GPSAltitude", "0x9286"); value2 = valor.
        if (text.isEmpty())
            return {};
        if (op == QLatin1String("exists") || op == QLatin1String("notExists")) {
            params << text;
            return QStringLiteral("p.id %1 IN (SELECT photo_id FROM exif_tag WHERE name = ? COLLATE NOCASE)")
                .arg(op == QLatin1String("notExists") ? QStringLiteral("NOT") : QString());
        }
        params << text;
        if (op == QLatin1String("eq"))
            params << rule.value2.toString();
        else
            params << u'%' + escapeLike(rule.value2.toString()) + u'%';
        return QStringLiteral("p.id IN (SELECT photo_id FROM exif_tag WHERE name = ? COLLATE NOCASE AND value %1)")
            .arg(op == QLatin1String("eq") ? QStringLiteral("= ? COLLATE NOCASE") : QStringLiteral("LIKE ? ESCAPE '\\'"));
    }

    if (op == QLatin1String("empty"))
        return type == QLatin1String("number") ? QStringLiteral("%1 IS NULL").arg(expr)
                                               : QStringLiteral("coalesce(%1, '') = ''").arg(expr);
    if (op == QLatin1String("notEmpty"))
        return type == QLatin1String("number") ? QStringLiteral("%1 IS NOT NULL").arg(expr)
                                               : QStringLiteral("coalesce(%1, '') != ''").arg(expr);
    if (type == QLatin1String("bool"))
        return QStringLiteral("coalesce(%1, 0) %2 0").arg(expr, op == QLatin1String("isTrue") ? QStringLiteral("!=") : QStringLiteral("="));

    if (type == QLatin1String("text") || type == QLatin1String("enum")) {
        if (op == QLatin1String("eq") || op == QLatin1String("ne")) {
            params << text;
            return QStringLiteral("coalesce(%1, '') %2 ? COLLATE NOCASE").arg(expr, op == QLatin1String("eq") ? QStringLiteral("=") : QStringLiteral("!="));
        }
        if (text.isEmpty())
            return {};
        QString pattern = escapeLike(text);
        if (op == QLatin1String("contains") || op == QLatin1String("notContains"))
            pattern = u'%' + pattern + u'%';
        else if (op == QLatin1String("startsWith"))
            pattern += u'%';
        else if (op == QLatin1String("endsWith"))
            pattern = u'%' + pattern;
        params << pattern;
        return QStringLiteral("coalesce(%1, '') %2 LIKE ? ESCAPE '\\'")
            .arg(expr, op == QLatin1String("notContains") ? QStringLiteral("NOT") : QString());
    }

    // number / date: comparação; date compara "yyyy-MM-dd" como texto.
    const bool isDate = type == QLatin1String("date");
    auto value = [&](const QVariant &v, bool *ok) -> QVariant {
        if (isDate) {
            const QDate d = QDate::fromString(v.toString().left(10), Qt::ISODate);
            *ok = d.isValid();
            return d.toString(Qt::ISODate);
        }
        const double n = v.toDouble(ok);
        return n;
    };
    bool ok1 = false, ok2 = false;
    const QVariant a = value(rule.value, &ok1);
    if (!ok1)
        return {};
    if (op == QLatin1String("between")) {
        const QVariant b = value(rule.value2, &ok2);
        if (!ok2)
            return {};
        params << a << b;
        return QStringLiteral("%1 BETWEEN ? AND ?").arg(expr);
    }
    static const QHash<QString, QString> sqlOps{
        {QStringLiteral("eq"), QStringLiteral("=")},   {QStringLiteral("ne"), QStringLiteral("!=")},
        {QStringLiteral("gt"), QStringLiteral(">")},   {QStringLiteral("gte"), QStringLiteral(">=")},
        {QStringLiteral("lt"), QStringLiteral("<")},   {QStringLiteral("lte"), QStringLiteral("<=")},
        {QStringLiteral("before"), QStringLiteral("<")}, {QStringLiteral("after"), QStringLiteral(">")},
    };
    if (!sqlOps.contains(op))
        return {};
    params << a;
    // Igualdade de números decimais (abertura 2.8, megapixels) com folga.
    if (!isDate && op == QLatin1String("eq"))
        return QStringLiteral("abs(%1 - ?) < 0.005").arg(expr);
    return QStringLiteral("%1 %2 ?").arg(expr, sqlOps.value(op));
}

} // namespace

QVariantList Catalog::filterFields()
{
    QVariantList out;
    for (const Field &f : kFields) {
        QVariantMap m{{QStringLiteral("key"), QString::fromLatin1(f.key)},
                      {QStringLiteral("type"), QString::fromLatin1(f.type)},
                      {QStringLiteral("group"), QString::fromLatin1(f.group)},
                      {QStringLiteral("ops"), opsByType().value(QString::fromLatin1(f.type))}};
        if (f.choices)
            m.insert(QStringLiteral("choices"), QString::fromLatin1(f.choices).split(u'|'));
        out << m;
    }
    return out;
}

QList<FilterRule> Catalog::rulesFromVariant(const QVariantList &rules)
{
    QList<FilterRule> out;
    for (const QVariant &v : rules) {
        const QVariantMap m = v.toMap();
        out.append({m.value(QStringLiteral("field")).toString(), m.value(QStringLiteral("op")).toString(),
                    m.value(QStringLiteral("value")), m.value(QStringLiteral("value2"))});
    }
    return out;
}

QString Catalog::rulesFilter(const QList<FilterRule> &rules, bool matchAll, QVariantList &params) const
{
    QStringList conditions;
    for (const FilterRule &rule : rules) {
        const QString c = ruleCondition(rule, params);
        if (!c.isEmpty())
            conditions << u'(' + c + u')';
    }
    if (conditions.isEmpty())
        return {};
    return u'(' + conditions.join(matchAll ? QStringLiteral(" AND ") : QStringLiteral(" OR ")) + u')';
}

QVariantList Catalog::savedFilters() const
{
    QVariantList out;
    QSqlQuery q = m_db.exec(QStringLiteral("SELECT id, name, definition FROM saved_filter ORDER BY name COLLATE NOCASE"));
    while (q.next()) {
        const QJsonObject def = QJsonDocument::fromJson(q.value(2).toString().toUtf8()).object();
        out << QVariantMap{{QStringLiteral("id"), q.value(0)},
                           {QStringLiteral("name"), q.value(1)},
                           {QStringLiteral("matchAll"), def.value(QStringLiteral("match")).toString() != QLatin1String("any")},
                           {QStringLiteral("rules"), def.value(QStringLiteral("rules")).toArray().toVariantList()}};
    }
    return out;
}

QString Catalog::saveFilter(const QString &name, bool matchAll, const QVariantList &rules)
{
    const QString clean = name.trimmed();
    if (clean.isEmpty())
        throw ActionError(QStringLiteral("filter.emptyName"));
    const QJsonObject def{{QStringLiteral("match"), matchAll ? QStringLiteral("all") : QStringLiteral("any")},
                          {QStringLiteral("rules"), QJsonArray::fromVariantList(rules)}};
    const QString json = QString::fromUtf8(QJsonDocument(def).toJson(QJsonDocument::Compact));
    // O mesmo nome substitui o filtro salvo antes.
    const QString existing = m_db.scalar(QStringLiteral("SELECT id FROM saved_filter WHERE name = ? COLLATE NOCASE"), {clean}).toString();
    if (!existing.isEmpty()) {
        m_db.exec(QStringLiteral("UPDATE saved_filter SET definition = ? WHERE id = ?"), {json, existing});
        return existing;
    }
    const QString id = Database::newId();
    m_db.exec(QStringLiteral("INSERT INTO saved_filter(id, name, definition, created_at) VALUES(?, ?, ?, ?)"),
              {id, clean, json, Database::timestamp()});
    return id;
}

void Catalog::deleteFilter(const QString &filterId)
{
    m_db.exec(QStringLiteral("DELETE FROM saved_filter WHERE id = ?"), {filterId});
}

} // namespace pix
