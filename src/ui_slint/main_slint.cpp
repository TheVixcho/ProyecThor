#include "SlintApp.h"
#include <clocale>

int main(int /*argc*/, char* /*argv*/[]) {
    // Requerido por libmpv en sistemas con locale decimal con coma (es_CL, es_MX, es_ES)
    std::setlocale(LC_NUMERIC, "C");

    ProyecThor::SlintUI::SlintApp app;
    return app.Run();
}
