#include "configwindow.h"
#include "overlaycontroller.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

const char *kGifFilter = "Imágenes animadas (*.gif *.webp *.apng *.png);;Todos los archivos (*)";

// Recorta una QKeySequence a una sola combinación de teclas.
QString firstCombination(const QKeySequence &seq)
{
    if (seq.isEmpty())
        return QString();
    return QKeySequence(seq[0]).toString(QKeySequence::PortableText);
}

// Diálogo para crear o editar una variación.
class VariationDialog : public QDialog
{
public:
    VariationDialog(Variation *variation, QWidget *parent)
        : QDialog(parent)
        , m_variation(variation)
    {
        setWindowTitle(tr("Variación"));
        setMinimumWidth(430);

        m_nameEdit = new QLineEdit(variation->name, this);
        m_nameEdit->setPlaceholderText(tr("Ej.: Hablando, Enfadado, Sorpresa"));

        m_gifEdit = new QLineEdit(variation->gifPath, this);
        auto *browse = new QPushButton(tr("Examinar…"), this);
        connect(browse, &QPushButton::clicked, this, [this]() {
            const QString path = QFileDialog::getOpenFileName(
                this, tr("Elegir GIF"), m_gifEdit->text(), tr(kGifFilter));
            if (!path.isEmpty())
                m_gifEdit->setText(path);
        });
        auto *gifRow = new QHBoxLayout;
        gifRow->addWidget(m_gifEdit, 1);
        gifRow->addWidget(browse);

        m_hotkeyEdit = new QKeySequenceEdit(QKeySequence(variation->hotkey), this);
        auto *clearHotkey = new QPushButton(tr("Borrar"), this);
        connect(clearHotkey, &QPushButton::clicked, m_hotkeyEdit, &QKeySequenceEdit::clear);
        auto *hotkeyRow = new QHBoxLayout;
        hotkeyRow->addWidget(m_hotkeyEdit, 1);
        hotkeyRow->addWidget(clearHotkey);

        m_modeCombo = new QComboBox(this);
        m_modeCombo->addItem(tr("Conmutar: se queda fija hasta pulsar de nuevo"),
                             int(TriggerMode::Toggle));
        m_modeCombo->addItem(tr("Mantener: sólo mientras la tecla está pulsada"),
                             int(TriggerMode::Hold));
        m_modeCombo->setCurrentIndex(variation->mode == TriggerMode::Hold ? 1 : 0);

        auto *form = new QFormLayout;
        form->addRow(tr("Nombre:"), m_nameEdit);
        form->addRow(tr("GIF:"), gifRow);
        form->addRow(tr("Atajo global:"), hotkeyRow);
        form->addRow(tr("Comportamiento:"), m_modeCombo);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &VariationDialog::onAccept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
    }

private:
    void onAccept()
    {
        if (m_nameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Falta el nombre"),
                                 tr("Ponle un nombre a la variación."));
            return;
        }
        m_variation->name = m_nameEdit->text().trimmed();
        m_variation->gifPath = m_gifEdit->text().trimmed();
        m_variation->hotkey = firstCombination(m_hotkeyEdit->keySequence());
        m_variation->mode = TriggerMode(m_modeCombo->currentData().toInt());
        accept();
    }

    Variation *m_variation;
    QLineEdit *m_nameEdit;
    QLineEdit *m_gifEdit;
    QKeySequenceEdit *m_hotkeyEdit;
    QComboBox *m_modeCombo;
};

} // namespace

ConfigWindow::ConfigWindow(AppConfig *config, OverlayController *controller, QWidget *parent)
    : QWidget(parent)
    , m_config(config)
    , m_controller(controller)
{
    setWindowTitle(tr("PNGTuber de escritorio"));
    resize(760, 620);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildProfilesTab(), tr("Perfiles"));
    tabs->addTab(buildMonitorsTab(), tr("Monitores"));
    tabs->addTab(buildGeneralTab(), tr("General"));

    m_warningLabel = new QLabel(this);
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet("color:#b34700;");
    m_warningLabel->hide();

    auto *hideButton = new QPushButton(tr("Ocultar a la bandeja"), this);
    connect(hideButton, &QPushButton::clicked, this, &QWidget::hide);

    auto *bottom = new QHBoxLayout;
    bottom->addWidget(m_warningLabel, 1);
    bottom->addWidget(hideButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs, 1);
    layout->addLayout(bottom);

    refreshFromConfig();
}

QWidget *ConfigWindow::buildProfilesTab()
{
    auto *page = new QWidget;

    m_profileList = new QListWidget(page);
    m_profileList->setMaximumWidth(190);
    connect(m_profileList, &QListWidget::currentRowChanged, this, [this](int) {
        if (!m_loading)
            loadProfileIntoForm();
    });

    auto *addProfile = new QPushButton(tr("Añadir"), page);
    connect(addProfile, &QPushButton::clicked, this, [this]() {
        Profile p;
        p.name = tr("Perfil %1").arg(m_config->profiles.size() + 1);
        m_config->profiles.append(p);
        refreshProfileList();
        m_profileList->setCurrentRow(m_config->profiles.size() - 1);
        applyAndSave();
    });

    auto *renameProfile = new QPushButton(tr("Renombrar"), page);
    connect(renameProfile, &QPushButton::clicked, this, [this]() {
        Profile *p = currentProfile();
        if (!p)
            return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Renombrar perfil"),
                                                   tr("Nombre:"), QLineEdit::Normal, p->name, &ok);
        if (!ok || name.trimmed().isEmpty())
            return;
        const QString old = p->name;
        p->name = name.trimmed();
        for (MonitorAssignment &m : m_config->monitors)
            if (m.profileName == old)
                m.profileName = p->name;
        refreshProfileList();
        refreshMonitorTable();
        applyAndSave();
    });

    auto *removeProfile = new QPushButton(tr("Eliminar"), page);
    connect(removeProfile, &QPushButton::clicked, this, [this]() {
        const int row = m_profileList->currentRow();
        if (row < 0 || m_config->profiles.size() <= 1)
            return;
        const QString name = m_config->profiles.at(row).name;
        m_config->profiles.removeAt(row);
        for (MonitorAssignment &m : m_config->monitors)
            if (m.profileName == name)
                m.profileName = m_config->profiles.first().name;
        refreshProfileList();
        refreshMonitorTable();
        applyAndSave();
    });

    auto *profileButtons = new QHBoxLayout;
    profileButtons->addWidget(addProfile);
    profileButtons->addWidget(renameProfile);
    profileButtons->addWidget(removeProfile);

    auto *leftColumn = new QVBoxLayout;
    leftColumn->addWidget(new QLabel(tr("Perfiles"), page));
    leftColumn->addWidget(m_profileList, 1);
    leftColumn->addLayout(profileButtons);

    // --- GIF base y aspecto ---
    m_idleGifEdit = new QLineEdit(page);
    m_idleGifEdit->setPlaceholderText(tr("GIF que se muestra en reposo"));
    auto *browseIdle = new QPushButton(tr("Examinar…"), page);
    connect(browseIdle, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, tr("Elegir GIF base"), m_idleGifEdit->text(), tr(kGifFilter));
        if (path.isEmpty())
            return;
        m_idleGifEdit->setText(path);
        if (Profile *p = currentProfile()) {
            p->idleGif = path;
            applyAndSave();
        }
    });
    auto *idleRow = new QHBoxLayout;
    idleRow->addWidget(m_idleGifEdit, 1);
    idleRow->addWidget(browseIdle);

    m_scaleSlider = new QSlider(Qt::Horizontal, page);
    m_scaleSlider->setRange(10, 400);
    m_scaleSpin = new QSpinBox(page);
    m_scaleSpin->setRange(10, 400);
    m_scaleSpin->setSuffix(" %");
    connect(m_scaleSlider, &QSlider::valueChanged, m_scaleSpin, &QSpinBox::setValue);
    connect(m_scaleSpin, &QSpinBox::valueChanged, m_scaleSlider, &QSlider::setValue);
    connect(m_scaleSpin, &QSpinBox::valueChanged, this, [this](int v) {
        if (m_loading)
            return;
        if (Profile *p = currentProfile()) {
            p->scalePercent = v;
            applyAndSave();
        }
    });
    auto *scaleRow = new QHBoxLayout;
    scaleRow->addWidget(m_scaleSlider, 1);
    scaleRow->addWidget(m_scaleSpin);

    m_opacitySlider = new QSlider(Qt::Horizontal, page);
    m_opacitySlider->setRange(5, 100);
    connect(m_opacitySlider, &QSlider::valueChanged, this, [this](int v) {
        if (m_loading)
            return;
        if (Profile *p = currentProfile()) {
            p->opacityPercent = v;
            applyAndSave();
        }
    });

    m_alignCombo = new QComboBox(page);
    m_alignCombo->addItem(tr("Al principio de la barra"), int(Align::Start));
    m_alignCombo->addItem(tr("En el centro de la barra"), int(Align::Center));
    m_alignCombo->addItem(tr("Al final de la barra"), int(Align::End));

    m_barModeCombo = new QComboBox(page);
    m_barModeCombo->addItem(tr("Apoyado encima de la barra"), int(BarMode::Above));
    m_barModeCombo->addItem(tr("Centrado dentro de la barra"), int(BarMode::On));
    m_barModeCombo->addItem(tr("Ignorar la barra, pegado al borde"), int(BarMode::Screen));

    auto onPlacementChanged = [this]() {
        if (m_loading)
            return;
        if (Profile *p = currentProfile()) {
            p->align = Align(m_alignCombo->currentData().toInt());
            p->barMode = BarMode(m_barModeCombo->currentData().toInt());
            applyAndSave();
        }
    };
    connect(m_alignCombo, &QComboBox::currentIndexChanged, this, onPlacementChanged);
    connect(m_barModeCombo, &QComboBox::currentIndexChanged, this, onPlacementChanged);

    m_offsetXSpin = new QSpinBox(page);
    m_offsetXSpin->setRange(-8000, 8000);
    m_offsetXSpin->setSuffix(" px");
    m_offsetYSpin = new QSpinBox(page);
    m_offsetYSpin->setRange(-8000, 8000);
    m_offsetYSpin->setSuffix(" px");
    auto onOffsetChanged = [this]() {
        if (m_loading)
            return;
        if (Profile *p = currentProfile()) {
            p->offsetX = m_offsetXSpin->value();
            p->offsetY = m_offsetYSpin->value();
            applyAndSave();
        }
    };
    connect(m_offsetXSpin, &QSpinBox::valueChanged, this, onOffsetChanged);
    connect(m_offsetYSpin, &QSpinBox::valueChanged, this, onOffsetChanged);
    auto *offsetRow = new QHBoxLayout;
    offsetRow->addWidget(new QLabel(tr("X:"), page));
    offsetRow->addWidget(m_offsetXSpin);
    offsetRow->addSpacing(12);
    offsetRow->addWidget(new QLabel(tr("Y:"), page));
    offsetRow->addWidget(m_offsetYSpin);
    offsetRow->addStretch(1);

    m_clickThroughCheck = new QCheckBox(tr("Dejar pasar los clics del ratón"), page);
    connect(m_clickThroughCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_loading)
            return;
        if (Profile *p = currentProfile()) {
            p->clickThrough = on;
            applyAndSave();
        }
    });

    auto *appearance = new QGroupBox(tr("Aspecto y posición"), page);
    auto *appearanceForm = new QFormLayout(appearance);
    appearanceForm->addRow(tr("GIF base:"), idleRow);
    appearanceForm->addRow(tr("Escala:"), scaleRow);
    appearanceForm->addRow(tr("Opacidad:"), m_opacitySlider);
    appearanceForm->addRow(tr("Posición:"), m_alignCombo);
    appearanceForm->addRow(tr("Respecto a la barra:"), m_barModeCombo);
    appearanceForm->addRow(tr("Ajuste fino:"), offsetRow);
    appearanceForm->addRow(QString(), m_clickThroughCheck);

    // --- Variaciones ---
    m_variationTable = new QTableWidget(0, 4, page);
    m_variationTable->setHorizontalHeaderLabels(
        { tr("Nombre"), tr("GIF"), tr("Atajo"), tr("Modo") });
    m_variationTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_variationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_variationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_variationTable->verticalHeader()->hide();
    connect(m_variationTable, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int) { editVariation(row); });

    auto *addVariation = new QPushButton(tr("Añadir variación"), page);
    connect(addVariation, &QPushButton::clicked, this, [this]() {
        Profile *p = currentProfile();
        if (!p)
            return;
        Variation v;
        v.name = tr("Variación %1").arg(p->variations.size() + 1);
        VariationDialog dlg(&v, this);
        if (dlg.exec() != QDialog::Accepted)
            return;
        p->variations.append(v);
        refreshVariationTable();
        applyAndSave();
    });

    auto *editVariationBtn = new QPushButton(tr("Editar"), page);
    connect(editVariationBtn, &QPushButton::clicked, this,
            [this]() { editVariation(m_variationTable->currentRow()); });

    auto *removeVariation = new QPushButton(tr("Eliminar"), page);
    connect(removeVariation, &QPushButton::clicked, this, [this]() {
        Profile *p = currentProfile();
        const int row = m_variationTable->currentRow();
        if (!p || row < 0 || row >= p->variations.size())
            return;
        p->variations.removeAt(row);
        refreshVariationTable();
        applyAndSave();
    });

    auto *variationButtons = new QHBoxLayout;
    variationButtons->addWidget(addVariation);
    variationButtons->addWidget(editVariationBtn);
    variationButtons->addWidget(removeVariation);
    variationButtons->addStretch(1);

    auto *variations = new QGroupBox(tr("Variaciones y atajos"), page);
    auto *variationLayout = new QVBoxLayout(variations);
    variationLayout->addWidget(m_variationTable, 1);
    variationLayout->addLayout(variationButtons);

    auto *rightColumn = new QVBoxLayout;
    rightColumn->addWidget(appearance);
    rightColumn->addWidget(variations, 1);

    auto *layout = new QHBoxLayout(page);
    layout->addLayout(leftColumn);
    layout->addLayout(rightColumn, 1);
    return page;
}

QWidget *ConfigWindow::buildMonitorsTab()
{
    auto *page = new QWidget;

    m_monitorTable = new QTableWidget(0, 3, page);
    m_monitorTable->setHorizontalHeaderLabels({ tr("Monitor"), tr("Activo"), tr("Perfil") });
    m_monitorTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_monitorTable->verticalHeader()->hide();
    m_monitorTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *hint = new QLabel(
        tr("Activa el overlay en los monitores que quieras. Si eliges el mismo perfil "
           "en todos, verás el mismo PNGTuber repetido; si eliges perfiles distintos, "
           "cada pantalla tendrá el suyo con sus propios GIFs."),
        page);
    hint->setWordWrap(true);

    auto *layout = new QVBoxLayout(page);
    layout->addWidget(hint);
    layout->addWidget(m_monitorTable, 1);
    return page;
}

QWidget *ConfigWindow::buildGeneralTab()
{
    auto *page = new QWidget;

    m_clickThroughHotkey = new QKeySequenceEdit(page);
    m_visibilityHotkey = new QKeySequenceEdit(page);

    auto onGlobalHotkeyChanged = [this]() {
        if (m_loading)
            return;
        m_config->hotkeyToggleClickThrough = firstCombination(m_clickThroughHotkey->keySequence());
        m_config->hotkeyToggleVisibility = firstCombination(m_visibilityHotkey->keySequence());
        applyAndSave();
    };
    connect(m_clickThroughHotkey, &QKeySequenceEdit::editingFinished, this, onGlobalHotkeyChanged);
    connect(m_visibilityHotkey, &QKeySequenceEdit::editingFinished, this, onGlobalHotkeyChanged);

    m_startMinimizedCheck = new QCheckBox(tr("Arrancar con la ventana oculta en la bandeja"), page);
    connect(m_startMinimizedCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (m_loading)
            return;
        m_config->startMinimized = on;
        emit requestSave();
    });

    auto *form = new QFormLayout;
    form->addRow(tr("Alternar paso de clics:"), m_clickThroughHotkey);
    form->addRow(tr("Mostrar / ocultar overlay:"), m_visibilityHotkey);
    form->addRow(QString(), m_startMinimizedCheck);

    auto *pathLabel = new QLabel(tr("Configuración guardada en:\n%1").arg(AppConfig::filePath()), page);
    pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    pathLabel->setWordWrap(true);

    auto *layout = new QVBoxLayout(page);
    layout->addLayout(form);
    layout->addSpacing(16);
    layout->addWidget(pathLabel);
    layout->addStretch(1);
    return page;
}

Profile *ConfigWindow::currentProfile()
{
    const int row = m_profileList->currentRow();
    if (row < 0 || row >= m_config->profiles.size())
        return nullptr;
    return &m_config->profiles[row];
}

void ConfigWindow::refreshFromConfig()
{
    m_loading = true;
    refreshProfileList();
    if (m_profileList->currentRow() < 0 && m_profileList->count() > 0)
        m_profileList->setCurrentRow(0);
    refreshMonitorTable();
    m_clickThroughHotkey->setKeySequence(QKeySequence(m_config->hotkeyToggleClickThrough));
    m_visibilityHotkey->setKeySequence(QKeySequence(m_config->hotkeyToggleVisibility));
    m_startMinimizedCheck->setChecked(m_config->startMinimized);
    m_loading = false;
    loadProfileIntoForm();
    refreshHotkeyWarnings();
}

void ConfigWindow::refreshProfileList()
{
    const bool prev = m_loading;
    m_loading = true;
    const int row = m_profileList->currentRow();
    m_profileList->clear();
    for (const Profile &p : m_config->profiles)
        m_profileList->addItem(p.name);
    m_profileList->setCurrentRow(qBound(0, row, m_config->profiles.size() - 1));
    m_loading = prev;
}

void ConfigWindow::loadProfileIntoForm()
{
    Profile *p = currentProfile();
    if (!p)
        return;

    m_loading = true;
    m_idleGifEdit->setText(p->idleGif);
    m_scaleSpin->setValue(p->scalePercent);
    m_scaleSlider->setValue(p->scalePercent);
    m_opacitySlider->setValue(p->opacityPercent);
    m_alignCombo->setCurrentIndex(m_alignCombo->findData(int(p->align)));
    m_barModeCombo->setCurrentIndex(m_barModeCombo->findData(int(p->barMode)));
    m_offsetXSpin->setValue(p->offsetX);
    m_offsetYSpin->setValue(p->offsetY);
    m_clickThroughCheck->setChecked(p->clickThrough);
    m_loading = false;

    refreshVariationTable();
}

void ConfigWindow::refreshVariationTable()
{
    Profile *p = currentProfile();
    m_variationTable->setRowCount(0);
    if (!p)
        return;

    m_variationTable->setRowCount(p->variations.size());
    for (int i = 0; i < p->variations.size(); ++i) {
        const Variation &v = p->variations.at(i);
        m_variationTable->setItem(i, 0, new QTableWidgetItem(v.name));
        m_variationTable->setItem(i, 1, new QTableWidgetItem(v.gifPath));
        m_variationTable->setItem(i, 2, new QTableWidgetItem(v.hotkey));
        m_variationTable->setItem(i, 3, new QTableWidgetItem(
            v.mode == TriggerMode::Hold ? tr("Mantener") : tr("Conmutar")));
    }
}

void ConfigWindow::refreshMonitorTable()
{
    const QList<QScreen *> screens = QGuiApplication::screens();
    m_monitorTable->setRowCount(screens.size());

    for (int i = 0; i < screens.size(); ++i) {
        QScreen *s = screens.at(i);
        const QRect g = s->geometry();
        auto *nameItem = new QTableWidgetItem(
            QStringLiteral("%1  (%2×%3)").arg(s->name()).arg(g.width()).arg(g.height()));
        m_monitorTable->setItem(i, 0, nameItem);

        MonitorAssignment *assign = m_config->monitorByName(s->name());
        if (!assign) {
            MonitorAssignment m;
            m.screenName = s->name();
            m.enabled = (i == 0);
            m.profileName = m_config->profiles.isEmpty() ? QString()
                                                         : m_config->profiles.first().name;
            m_config->monitors.append(m);
            assign = m_config->monitorByName(s->name());
        }

        auto *check = new QCheckBox(m_monitorTable);
        check->setChecked(assign->enabled);
        const QString screenName = s->name();
        connect(check, &QCheckBox::toggled, this, [this, screenName](bool on) {
            if (m_loading)
                return;
            if (MonitorAssignment *m = m_config->monitorByName(screenName)) {
                m->enabled = on;
                applyAndSave();
            }
        });
        m_monitorTable->setCellWidget(i, 1, check);

        auto *combo = new QComboBox(m_monitorTable);
        for (const Profile &p : m_config->profiles)
            combo->addItem(p.name);
        combo->setCurrentText(assign->profileName);
        connect(combo, &QComboBox::currentTextChanged, this,
                [this, screenName](const QString &name) {
                    if (m_loading)
                        return;
                    if (MonitorAssignment *m = m_config->monitorByName(screenName)) {
                        m->profileName = name;
                        applyAndSave();
                    }
                });
        m_monitorTable->setCellWidget(i, 2, combo);
    }
}

void ConfigWindow::refreshHotkeyWarnings()
{
    if (!m_controller) {
        m_warningLabel->hide();
        return;
    }
    const QStringList failed = m_controller->failedHotkeys();
    if (failed.isEmpty()) {
        m_warningLabel->hide();
        return;
    }
    m_warningLabel->setText(
        tr("El sistema rechazó estos atajos (probablemente los usa otro programa): %1")
            .arg(failed.join(QStringLiteral(", "))));
    m_warningLabel->show();
}

void ConfigWindow::editVariation(int row)
{
    Profile *p = currentProfile();
    if (!p || row < 0 || row >= p->variations.size())
        return;
    Variation copy = p->variations.at(row);
    VariationDialog dlg(&copy, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    p->variations[row] = copy;
    refreshVariationTable();
    applyAndSave();
}

void ConfigWindow::applyAndSave()
{
    emit requestApply();
    emit requestSave();
    refreshHotkeyWarnings();
}

void ConfigWindow::closeEvent(QCloseEvent *event)
{
    // Cerrar la ventana no cierra el programa: sigue en la bandeja.
    emit requestSave();
    hide();
    event->ignore();
}
