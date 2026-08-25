#pragma once

#include "config.h"

#include <QWidget>

class QListWidget;
class QLineEdit;
class QSlider;
class QSpinBox;
class QComboBox;
class QCheckBox;
class QTableWidget;
class QLabel;
class QKeySequenceEdit;
class OverlayController;

// Ventana emergente que aparece al ejecutar el programa. Edita la configuración
// en vivo: cada cambio se aplica al instante a los overlays.
class ConfigWindow : public QWidget
{
    Q_OBJECT

public:
    ConfigWindow(AppConfig *config, OverlayController *controller, QWidget *parent = nullptr);

    void refreshFromConfig();

signals:
    void requestApply();  // reconstruir overlays y atajos
    void requestSave();

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QWidget *buildProfilesTab();
    QWidget *buildMonitorsTab();
    QWidget *buildGeneralTab();

    Profile *currentProfile();
    void loadProfileIntoForm();
    void refreshProfileList();
    void refreshVariationTable();
    void refreshMonitorTable();
    void refreshHotkeyWarnings();

    void applyAndSave();
    void editVariation(int row);
    void pushOffsetsToProfile();
    void applyOffsetToSlider(QSlider *slider, QLabel *valueLabel, int value);
    QString uniqueProfileName(const QString &wanted, int ignoreIndex = -1) const;

    AppConfig *m_config = nullptr;
    OverlayController *m_controller = nullptr;
    bool m_loading = false;

    QListWidget *m_profileList = nullptr;
    QLineEdit *m_idleGifEdit = nullptr;
    QSlider *m_scaleSlider = nullptr;
    QSpinBox *m_scaleSpin = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QComboBox *m_alignCombo = nullptr;
    QComboBox *m_barModeCombo = nullptr;
    QSlider *m_offsetXSlider = nullptr;
    QSlider *m_offsetYSlider = nullptr;
    QLabel *m_offsetXValue = nullptr;
    QLabel *m_offsetYValue = nullptr;
    QCheckBox *m_clickThroughCheck = nullptr;
    QTableWidget *m_variationTable = nullptr;

    QTableWidget *m_monitorTable = nullptr;

    QKeySequenceEdit *m_clickThroughHotkey = nullptr;
    QKeySequenceEdit *m_visibilityHotkey = nullptr;
    QCheckBox *m_startMinimizedCheck = nullptr;
    QLabel *m_warningLabel = nullptr;
};
