// Boxshelf PS5 - interface languages. SPDX-License-Identifier: GPL-3.0-or-later
#include "app/i18n.hpp"

#include <cstring>

namespace boxshelf::app
{
namespace
{
Lang g_lang = Lang::en;

struct Entry
{
    const char *en, *pt, *es;
};
// clang-format off
constexpr Entry kTable[] = {
    // tabs and wordmarks
    {"Others", "Outros", "Otros"},
    {"Media", "M\xc3\xad" "dia", "Multimedia"},
    {"Home", "In\xc3\xad" "cio", "Inicio"},
    {"Home console", "Console de mesa", "Consola de sobremesa"},
    {"Handheld", "Port\xc3\xa1til", "Port\xc3\xa1til"},
    {"Hybrid console", "Console h\xc3\xad" "brido", "Consola h\xc3\xad" "brida"},
    {"Motion console", "Console de movimento", "Consola de movimiento"},
    {"Apps & ports", "Apps e ports", "Apps y ports"},
    {"Streaming", "Streaming", "Streaming"},
    {"Continue & favorites", "Continuar e favoritos", "Continuar y favoritos"},
    {"GAME", "JOGO", "JUEGO"},
    {"GAMES", "JOGOS", "JUEGOS"},
    // hints
    {"System", "Sistema", "Sistema"},
    {"Flip", "Virar", "Girar"},
    {"Details", "Detalhes", "Detalles"},
    {"Play", "Jogar", "Jugar"},
    {"Set up", "Configurar", "Configurar"},
    {"Inspect", "Inspecionar", "Inspeccionar"},
    {"Shuffle", "Aleat\xc3\xb3rio", "Aleatorio"},
    {"Settings", "Ajustes", "Ajustes"},
    {"Museum", "Museu", "Museo"},
    {"Menu", "Menu", "Men\xc3\xba"},
    {"Sort", "Ordenar", "Ordenar"},
    {"HOLD", "SEGURE", "MANT\xc3\x89N"},
    {"Select", "Escolher", "Elegir"},
    {"Back", "Voltar", "Volver"},
    {"Close", "Fechar", "Cerrar"},
    // the info line
    {"Emulator", "Emulador", "Emulador"},
    {"Open settings", "Abrir ajustes", "Abrir ajustes"},
    {"No %s games yet", "Nenhum jogo de %s ainda", "A\xc3\xban no hay juegos de %s"},
    {"Cross opens %s settings to add a games folder", "\xe2\x9c\x95 abre os ajustes de %s para adicionar uma pasta de jogos",
     "\xe2\x9c\x95 abre los ajustes de %s para a\xc3\xb1" "adir una carpeta de juegos"},
    {"Lands on a random game", "Para em um jogo aleat\xc3\xb3rio", "Elige un juego al azar"},
    {"No games here yet", "Nenhum jogo aqui ainda", "A\xc3\xban no hay juegos aqu\xc3\xad"},
    {"NOTHING ON THE SHELF YET", "NADA NA ESTANTE AINDA", "NADA EN LA ESTANTER\xc3\x8d" "A A\xc3\x9aN"},
    {"Installed games, then the folders your emulators use", "Jogos instalados, depois as pastas dos seus emuladores",
     "Juegos instalados y luego las carpetas de tus emuladores"},
    {"Put your games in Emulation/roms/<system> on your drive and open Vitrine again.",
     "Coloque seus jogos em Emulation/roms/<sistema> no seu disco e abra o Vitrine de novo.",
     "Pon tus juegos en Emulation/roms/<sistema> en tu disco y abre Vitrine otra vez."},
    {"Reading your library", "Lendo sua biblioteca", "Leyendo tu biblioteca"},
    {"Connect your emulators and game folders", "Conecte seus emuladores e pastas de jogos",
     "Conecta tus emuladores y carpetas de juegos"},
    {"Looking for your games", "Procurando seus jogos", "Buscando tus juegos"},
    {"Reading the installed games", "Lendo os jogos instalados", "Leyendo los juegos instalados"},
    {"Reading the emulators' settings", "Lendo os ajustes dos emuladores", "Leyendo los ajustes de los emuladores"},
    {"Looking through your game folders", "Olhando suas pastas de jogos", "Revisando tus carpetas de juegos"},
    {"Matching box art", "Encontrando as capas", "Buscando las car\xc3\xa1tulas"},
    // the made-up back
    {"Region", "Regi\xc3\xa3o", "Regi\xc3\xb3n"},
    {"File", "Arquivo", "Archivo"},
    {"Folder", "Pasta", "Carpeta"},
    {"Serial", "Serial", "Serie"},
    {"Type", "Tipo", "Tipo"},
    {"Title ID", "ID do t\xc3\xadtulo", "ID del t\xc3\xadtulo"},
    {"Unknown", "Desconhecida", "Desconocida"},
    {"Homebrew", "Homebrew", "Homebrew"},
    {"Streaming app", "App de streaming", "App de streaming"},
    {"%s game", "Jogo de %s", "Juego de %s"},
    // playing
    {"Checking the console", "Verificando o console", "Comprobando la consola"},
    {"Could not start %s", "N\xc3\xa3o foi poss\xc3\xadvel iniciar %s", "No se pudo iniciar %s"},
    {"Starting games needs the console", "Abrir jogos s\xc3\xb3 funciona no console", "Abrir juegos necesita la consola"},
    {"Could not reach the console. Try again.", "N\xc3\xa3o foi poss\xc3\xadvel falar com o console. Tente de novo.",
     "No se pudo contactar con la consola. Int\xc3\xa9ntalo de nuevo."},
    {"%s is already open", "%s j\xc3\xa1 est\xc3\xa1 aberto", "%s ya est\xc3\xa1 abierto"},
    {"Another game is open. Close it from the PS5 menu.", "Outro jogo est\xc3\xa1 aberto. Feche-o pelo menu do PS5.",
     "Hay otro juego abierto. Ci\xc3\xa9rralo desde el men\xc3\xba de PS5."},
    {"Opening %s", "Abrindo %s", "Abriendo %s"},
    {"Opening PS5SX2 with %s", "Abrindo o PS5SX2 com %s", "Abriendo PS5SX2 con %s"},
    {"Starting %s", "Iniciando %s", "Iniciando %s"},
    {"Direct start unavailable. Opening the emulator.", "In\xc3\xad" "cio direto indispon\xc3\xadvel. Abrindo o emulador.",
     "Inicio directo no disponible. Abriendo el emulador."},
    {"The request failed. Your library is untouched.", "O pedido falhou. Sua biblioteca continua intacta.",
     "La solicitud fall\xc3\xb3. Tu biblioteca sigue intacta."},
    {"Request sent. Waiting for the game", "Pedido enviado. Aguardando o jogo", "Solicitud enviada. Esperando el juego"},
    {"Waiting for the PS5", "Aguardando o PS5", "Esperando a la PS5"},
    {"%s is starting", "%s est\xc3\xa1 abrindo", "%s se est\xc3\xa1 abriendo"},
    {"Not confirmed. Check the game in the PS5 menu.", "N\xc3\xa3o confirmado. Confira o jogo no menu do PS5.",
     "Sin confirmar. Comprueba el juego en el men\xc3\xba de PS5."},
    {"Still starting the last game", "Ainda abrindo o \xc3\xbaltimo jogo", "Todav\xc3\xad" "a abriendo el \xc3\xbaltimo juego"},
    {"No emulator for %s on this console", "Nenhum emulador de %s neste console", "No hay emulador de %s en esta consola"},
    // the quick menu
    {"Move to shelf", "Mover para a aba", "Mover a la pesta\xc3\xb1" "a"},
    {"Language", "Idioma", "Idioma"},
    {"%s added to favorites", "%s adicionado aos favoritos", "%s a\xc3\xb1" "adido a favoritos"},
    {"Removed %s from favorites", "%s removido dos favoritos", "%s quitado de favoritos"},
    {"played today", "jogado hoje", "jugado hoy"},
    {"played yesterday", "jogado ontem", "jugado ayer"},
    {"played %s days ago", "jogado h\xc3\xa1 %s dias", "jugado hace %s d\xc3\xad" "as"},
    {"Favorite", "Favorito", "Favorito"},
    {"Filter", "Filtrar", "Filtrar"},
    {"Turn", "Girar", "Girar"},
    {"Released", "Lan\xc3\xa7" "amento", "Lanzamiento"},
    {"Developer", "Desenvolvedora", "Desarrolladora"},
    {"Publisher", "Distribuidora", "Distribuidora"},
    {"Genre", "G\xc3\xaanero", "G\xc3\xa9nero"},
    {"Players", "Jogadores", "Jugadores"},
    {"(co-op)", "(cooperativo)", "(cooperativo)"},
    {"Rating", "Classifica\xc3\xa7\xc3\xa3o", "Clasificaci\xc3\xb3n"},
    {"Community score", "Nota da comunidade", "Nota de la comunidad"},
    {"Score", "Nota", "Nota"},
    {"About this game", "Sobre o jogo", "Sobre el juego"},
    {"1 player", "1 jogador", "1 jugador"},
    {"%s players", "%s jogadores", "%s jugadores"},
    {"Release year", "Ano de lan\xc3\xa7" "amento", "A\xc3\xb1o de lanzamiento"},
    {"2+ players", "2+ jogadores", "2+ jugadores"},
    {"Co-op", "Cooperativo", "Cooperativo"},
    {"to go back", "para voltar", "para volver"},
    {"Zoom", "Zoom", "Zoom"},
    {"Open case", "Abrir a caixa", "Abrir la caja"},
    {"Take disc out", "Tirar o disco", "Sacar el disco"},
    {"Put back", "Guardar", "Devolver"},
    {"Close case", "Fechar a caixa", "Cerrar la caja"},
    {"Put disc back", "Guardar o disco", "Guardar el disco"},
    {"%s of", "%s de", "%s de"},
    {"Never", "Nunca", "Nunca"},
    {"Today", "Hoje", "Hoy"},
    {"Yesterday", "Ontem", "Ayer"},
    {"%s days ago", "h\xc3\xa1 %s dias", "hace %s d\xc3\xad" "as"},
    {"Launches", "Aberturas", "Aperturas"},
    {"Last time", "\xc3\x9altima vez", "\xc3\x9altima vez"},
    {"Game info", "Informa\xc3\xa7\xc3\xb5" "es do jogo", "Informaci\xc3\xb3n del juego"},
    {"Format", "Formato", "Formato"},
    {"Your copy", "Sua c\xc3\xb3pia", "Tu copia"},
    {"Front cover", "Capa", "Portada"},
    {"Back cover", "Verso", "Reverso"},
    {"Plays in %s. Close the emulator to come back here.", "Abre no %s. Feche o emulador para voltar aqui.",
     "Se abre en %s. Cierra el emulador para volver aqu\xc3\xad."},
    {"Add to favorites", "Adicionar aos favoritos", "A\xc3\xb1" "adir a favoritos"},
    {"Remove from favorites", "Remover dos favoritos", "Quitar de favoritos"},
    {"Last played", "Jogados por \xc3\xbaltimo", "Jugados por \xc3\xbaltima vez"},
    {"Most played", "Mais jogados", "M\xc3\xa1s jugados"},
    {"All games", "Todos os jogos", "Todos los juegos"},
    {"Favorites", "Favoritos", "Favoritos"},
    {"Played", "Jogados", "Jugados"},
    {"Not played yet", "Ainda n\xc3\xa3o jogados", "A\xc3\xban sin jugar"},
    {"NOTHING MATCHES THE FILTER", "NENHUM JOGO NO FILTRO", "NING\xc3\x9aN JUEGO EN EL FILTRO"},
    {"Menu > Filter > All games shows them again.",
     "Menu > Filtrar > Todos os jogos mostra todos de novo.",
     "Men\xc3\xba > Filtrar > Todos los juegos los muestra otra vez."},
    {"Box backs", "Versos das caixas", "Reversos de las cajas"},
    {"Download", "Baixar", "Descargar"},
    {"Off", "Desligado", "Desactivado"},
    {"On", "Ligado", "Activado"},
    {"Box art", "Capas", "Car\xc3\xa1tulas"},
    {"Up to date", "Em dia", "Al d\xc3\xad" "a"},
    {"Downloading %s", "Baixando %s", "Descargando %s"},
    {"Find missing", "Buscar as que faltam", "Buscar las que faltan"},
    {"Download all again", "Baixar todas de novo", "Descargar todas otra vez"},
    {"Box art is already downloading", "As capas j\xc3\xa1 est\xc3\xa3o sendo baixadas", "Las car\xc3\xa1tulas ya se est\xc3\xa1n descargando"},
    {"Looking for the missing box art", "Procurando as capas que faltam", "Buscando las car\xc3\xa1tulas que faltan"},
    {"Downloading all the box art again", "Baixando todas as capas de novo", "Descargando todas las car\xc3\xa1tulas otra vez"},
    {"Box art %s", "Capas %s", "Car\xc3\xa1tulas %s"},
    {"Box art up to date", "Capas em dia", "Car\xc3\xa1tulas al d\xc3\xad" "a"},
    {"Shelf", "Aba", "Pesta\xc3\xb1" "a"},
    {"Moved to %s", "Movido para %s", "Movido a %s"},
    {"Back on %s", "De volta em %s", "De vuelta en %s"},
    {"Its own shelf", "Aba original", "Pesta\xc3\xb1" "a original"},
    {"Could not save the settings", "N\xc3\xa3o foi poss\xc3\xadvel salvar os ajustes", "No se pudieron guardar los ajustes"},
};
// clang-format on
} // namespace

Lang language() noexcept
{
    return g_lang;
}

void set_language(Lang lang) noexcept
{
    g_lang = lang;
}

Lang language_from_system(int l) noexcept
{
    // SceSystemParamLang: 7 Portuguese (Portugal), 17 Portuguese (Brazil), 3 Spanish, 20 Spanish (Latin America)
    if (l == 7 || l == 17)
        return Lang::pt;
    if (l == 3 || l == 20)
        return Lang::es;
    return Lang::en;
}

std::string_view language_code(Lang lang) noexcept
{
    return lang == Lang::pt ? "pt" : lang == Lang::es ? "es" : "en";
}

Lang language_from_code(std::string_view code, Lang fallback) noexcept
{
    if (code == "pt")
        return Lang::pt;
    if (code == "es")
        return Lang::es;
    if (code == "en")
        return Lang::en;
    return fallback;
}

const char *language_name(Lang lang) noexcept
{
    return lang == Lang::pt ? "Portugu\xc3\xaas" : lang == Lang::es ? "Espa\xc3\xb1ol" : "English";
}

const char *tr(const char *english) noexcept
{
    if (g_lang == Lang::en || !english)
        return english;
    for (const auto &e : kTable)
        if (std::strcmp(e.en, english) == 0)
            return g_lang == Lang::pt ? e.pt : e.es;
    return english;
}

std::string trf(const char *english, std::string_view arg)
{
    const std::string pattern = tr(english);
    const auto at = pattern.find("%s");
    if (at == std::string::npos)
        return pattern;
    return pattern.substr(0, at) + std::string(arg) + pattern.substr(at + 2);
}
} // namespace boxshelf::app
