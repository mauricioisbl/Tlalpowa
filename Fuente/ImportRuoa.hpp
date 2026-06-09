#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ImportRuoa {

struct RuoaCredentials {
    std::string usuario;
    std::string password;
    // Nombre y correo que el formulario PEMBU envia realmente al endpoint.
    // Si estan vacios se infieren de usuario/password sin persistirlos.
    std::string nombre_publico;
    std::string correo;
};

struct RuoaDownloadOptions {
    std::filesystem::path destino_raiz;
    int anio_mas_antiguo = 1997;
    int anio_mas_reciente = 2026;
    int pausa_ms_entre_csv = 1000;
    int intentos_por_csv = 3;
    std::uintmax_t csv_min_bytes = 64;
    bool conservar_csv_valido = true;
    bool estrictamente_reciente_a_antiguo = true;
    // En modo transversal se itera por ronda temporal: año reciente -> antiguo,
    // mes diciembre -> enero y, dentro de cada mes, todas las estaciones.
    bool transversal_por_mes = true;
    std::atomic_bool* cancelar = nullptr;
};

struct RuoaStationProgress {
    std::string estacion;
    std::string etiqueta;
    int completados = 0;
    int total = 0;
    int descargados = 0;
    int omitidos_validos = 0;
    int fallidos = 0;
};

struct RuoaProgress {
    int completados = 0;
    int total = 0;
    int descargados = 0;
    int utilizables = 0;
    int omitidos_validos = 0;
    int fallidos = 0;
    int anio = 0;
    int mes = 0;
    std::string estacion;
    std::string etiqueta;
    std::filesystem::path destino;
    std::string fase;
    std::vector<RuoaStationProgress> estaciones;
};

using RuoaProgressCallback = std::function<void(const RuoaProgress&)>;

bool descargar_csvs_pembu(const RuoaCredentials& credenciales,
                          const RuoaDownloadOptions& opciones,
                          nlohmann::json& auditoria,
                          RuoaProgressCallback progreso = {});

const std::vector<std::string>& estaciones_pembu();

}  // namespace ImportRuoa
