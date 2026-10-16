// SPDX-License-Identifier: BSD-2-Clause
#pragma once

// Idioma da interface. Os textos do código estão em inglês (o padrão e o
// fallback de todo o sistema); as traduções (i18n/photoidx_*.ts) são
// compiladas em :/i18n. Resolução: escolha salva -> idioma do SO -> inglês.

#include <QString>
#include <QVariantList>

namespace pix::app {

/// Idioma salvo nas preferências ("" = seguir o sistema).
QString savedLanguage();
void saveLanguage(const QString &code);

/// Instala os tradutores do app e do Qt para ``code`` (ou o do sistema).
void installLanguage(const QString &code);

/// Idioma efetivamente em uso ("en", "pt_BR").
QString activeLanguage();

/// [{code, name}] com o nome de cada idioma na própria língua.
QVariantList availableLanguages();

} // namespace pix::app
