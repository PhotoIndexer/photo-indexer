// SPDX-License-Identifier: BSD-2-Clause
#include "Language.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>
#include <QVariantMap>

namespace pix::app {

namespace {

struct Translators
{
    QTranslator app;
    QTranslator qt;
    QString active = QStringLiteral("en");
};

Translators &translators()
{
    // Nunca destruído: threads de trabalho ainda podem traduzir na saída.
    static Translators *t = new Translators;
    return *t;
}

// Inglês é a língua do código: não precisa de arquivo (só plurais). Para um
// idioma novo, acrescente aqui e o .ts no src/app/CMakeLists.txt.
const char *const kLanguages[][2] = {
    {"en", "English"},
    {"pt_BR", "Português (Brasil)"},
};

} // namespace

QString savedLanguage()
{
    return QSettings().value(QStringLiteral("ui/language")).toString();
}

void saveLanguage(const QString &code)
{
    QSettings().setValue(QStringLiteral("ui/language"), code);
}

void installLanguage(const QString &code)
{
    Translators &t = translators();
    QCoreApplication::removeTranslator(&t.app);
    QCoreApplication::removeTranslator(&t.qt);

    const QLocale locale = code.isEmpty() ? QLocale::system() : QLocale(code);
    // Procura photoidx_pt_BR, depois photoidx_pt, para cada idioma
    // preferido do sistema. Sem tradução: inglês, cujo arquivo só traz plurais.
    if (t.app.load(locale, QStringLiteral("photoidx"), QStringLiteral("_"), QStringLiteral(":/i18n"))
        || t.app.load(QStringLiteral(":/i18n/photoidx_en.qm"))) {
        QCoreApplication::installTranslator(&t.app);
        t.active = t.app.language().isEmpty() ? QStringLiteral("en") : t.app.language();
    } else {
        t.active = QStringLiteral("en");
    }
    t.active.replace(u'-', u'_');
    // Textos do próprio Qt (diálogos de arquivo), quando instalados.
    if (t.active != QLatin1String("en")
        && t.qt.load(locale, QStringLiteral("qtbase"), QStringLiteral("_"),
                     QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&t.qt);
    QLocale::setDefault(t.active == QLatin1String("en") ? QLocale(QLocale::English) : QLocale(t.active));
}

QString activeLanguage()
{
    return translators().active;
}

QVariantList availableLanguages()
{
    QVariantList out;
    for (const auto &l : kLanguages)
        out << QVariantMap{{QStringLiteral("code"), QString::fromLatin1(l[0])},
                           {QStringLiteral("name"), QString::fromUtf8(l[1])}};
    return out;
}

} // namespace pix::app
