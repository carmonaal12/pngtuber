// Prueba funcional de la ventana de configuración.
// Comprueba los tres puntos que más se han roto históricamente:
//   1. La escala arranca en 100 %.
//   2. El ajuste fino son barras deslizantes, no cajas de texto.
//   3. Los perfiles nuevos aparecen al momento en la pestaña de monitores.
//
// Se ejecuta sin pantalla real:  QT_QPA_PLATFORM=offscreen ./ui_test
#include "../src/configwindow.h"
#include "../src/overlaycontroller.h"

#include <QApplication>
#include <QComboBox>
#include <QDebug>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>

static int failures = 0;

static void check(const char *label, bool ok)
{
    qInfo().noquote() << (ok ? "  OK  " : " FALLO") << label;
    if (!ok)
        ++failures;
}

// Busca un botón por su texto visible.
static QPushButton *buttonNamed(QWidget *root, const QString &text)
{
    const auto buttons = root->findChildren<QPushButton *>();
    for (QPushButton *b : buttons)
        if (b->text() == text)
            return b;
    return nullptr;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    AppConfig config;
    Profile principal;
    principal.name = QStringLiteral("Principal");
    config.profiles.append(principal);

    OverlayController controller(&config);
    ConfigWindow window(&config, &controller);

    // --- 1. Escala inicial ---
    const auto spins = window.findChildren<QSpinBox *>();
    QSpinBox *scaleSpin = spins.isEmpty() ? nullptr : spins.first();
    check("existe la caja de escala", scaleSpin != nullptr);
    if (scaleSpin)
        check("la escala arranca en 100 %", scaleSpin->value() == 100);

    // --- 2. Ajuste fino con barras deslizantes ---
    // Escala + opacidad + X + Y = cuatro sliders; ninguna caja numérica de
    // coordenadas (la única QSpinBox que queda es la de la escala).
    const auto sliders = window.findChildren<QSlider *>();
    check("hay cuatro barras deslizantes (escala, opacidad, X, Y)", sliders.size() == 4);
    check("el ajuste fino ya no usa cajas numéricas", spins.size() == 1);

    // --- 3. Perfiles nuevos visibles en la pestaña de monitores ---
    QTableWidget *monitorTable = nullptr;
    const auto tables = window.findChildren<QTableWidget *>();
    for (QTableWidget *t : tables)
        if (t->columnCount() == 3)
            monitorTable = t;
    check("existe la tabla de monitores", monitorTable != nullptr);

    // En un entorno sin monitores (algunos servidores de integración continua)
    // la tabla queda vacía y esas comprobaciones no aplican.
    const bool hasMonitors = monitorTable && monitorTable->rowCount() > 0;
    if (!hasMonitors)
        qInfo().noquote() << " (aviso) sin monitores detectados: se omite la tabla";

    auto profileCombo = [&](int row) -> QComboBox * {
        if (!hasMonitors || row >= monitorTable->rowCount())
            return nullptr;
        return qobject_cast<QComboBox *>(monitorTable->cellWidget(row, 2));
    };

    if (hasMonitors) {
        check("el monitor tiene un desplegable de perfiles", profileCombo(0) != nullptr);
        if (QComboBox *c = profileCombo(0))
            check("de entrada aparece un único perfil", c->count() == 1);
    }

    QPushButton *add = buttonNamed(&window, QStringLiteral("Añadir"));
    check("existe el botón de añadir perfil", add != nullptr);
    if (add) {
        add->click();
        add->click();
    }

    check("se han creado tres perfiles", config.profiles.size() == 3);
    if (QComboBox *c = profileCombo(0)) {
        check("el desplegable de monitores lista los tres perfiles", c->count() == 3);
        check("y conserva el perfil asignado",
              c->currentText() == QStringLiteral("Principal"));
    }

    // Los nombres duplicados se desambiguan solos.
    check("los perfiles tienen nombres distintos",
          config.profiles.at(1).name != config.profiles.at(2).name);

    // Cambiar el perfil de un monitor desde el desplegable debe guardarse.
    if (QComboBox *c = profileCombo(0)) {
        c->setCurrentIndex(2);
        check("cambiar de perfil actualiza la asignación del monitor",
              !config.monitors.isEmpty()
                  && config.monitors.first().profileName == config.profiles.at(2).name);
    }

    if (failures == 0)
        qInfo().noquote() << "Todas las comprobaciones han pasado.";
    return failures == 0 ? 0 : 1;
}
