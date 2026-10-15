/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2015 Travis Nickles <nickles.travis@gmail.com>
 * Copyright (C) 2020 Jagoda Górska <juliagoda.pl@protonmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "pad/paddialogpresentation.h"

#include "common.h"
#include "eventhandlerfactory.h"

#include <SDL2/SDL_gamecontroller.h>
#include <SDL2/SDL_version.h>

#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QResource>
#include <QStringList>
#include <QTextStream>
#include <QtGlobal>

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AboutDialog)
{
    ui->setupUi(this);
    ui->versionLabel->setText(PadderCommon::displayVersion);
    fillInfoTextBrowser();
    connect(ui->infoTextBrowser, &QTextBrowser::anchorClicked, this, [this](const QUrl &) {
        ui->tabWidget->setCurrentIndex(ui->tabWidget->indexOf(ui->license));
    });
    QFile licenseFile(QStringLiteral(":/pad/GPL-3.0.txt"));
    QString legalNotice = QStringLiteral("Pad\n"
        "Copyright (C) 2026 Pad\n\n"
        "Pad is free software, released under the GNU General Public License version 3 or later. "
        "You can copy, change and share it under that license. It comes with no warranty.\n\n"
        "Source code: github.com/davaughnl/Pad\n\n"
        "Pad is based on AntiMicroX. Copyright (C) 2015 Travis Nickles, Copyright (C) 2020 Jagoda G\u00f3rska. "
        "Qt and SDL are used under their own licenses.\n\n"
        "The full license text follows.\n\n");
    if (licenseFile.open(QIODevice::ReadOnly | QIODevice::Text))
        legalNotice += QString::fromUtf8(licenseFile.readAll());
    ui->textBrowser_2->setPlainText(legalNotice);
    ui->tabWidget->removeTab(ui->tabWidget->indexOf(ui->changelog)); // About keeps Info and License only.
    PadUi::polishAboutDialog(this);
}

AboutDialog::~AboutDialog() { delete ui; }

void AboutDialog::fillInfoTextBrowser()
{
    // Keep attribution and the full GPL notice in Legal, not the product overview.
    ui->infoTextBrowser->setOpenLinks(false);
    ui->infoTextBrowser->setHtml(
        QStringLiteral("<p style=\"font-size:15px\">%1</p>"
                       "<p>%2</p><p><a href=\"#licenses\" style=\"color:#f4f4f5\">%3</a></p><p>%4</p>")
            .arg(tr("About Pad"),
                 tr("Pad is a modern controller mapping application designed for a simple, reliable, and highly configurable experience."),
                 tr("Legal"),
                 tr("Copyright (C) 2026 Pad")));
}

void AboutDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();

    QDialog::changeEvent(event);
}

void AboutDialog::retranslateUi()
{
    const QString legalNotice = ui->textBrowser_2->toPlainText();
    ui->retranslateUi(this);
    ui->textBrowser_2->setPlainText(legalNotice);
    fillInfoTextBrowser();

    ui->versionLabel->setText(PadderCommon::displayVersion);
}
