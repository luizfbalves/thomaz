#pragma once

#include <string>

namespace brls { class Activity; }

namespace thomaz {

// Mostra "@usuario" (sessão atual) no canto direito do header do AppletFrame.
// Chamado em onContentAvailable de cada activity, para o nome aparecer
// globalmente em todas as telas. No-op se não houver sessão ou header.
void install_header_username(brls::Activity* activity);

// Registra a ação de ajuda no botão "-" (BUTTON_BACK) do rodapé: abre um
// diálogo explicando como a tela funciona. `frameId` é o id do AppletFrame da
// tela; `bodyKey` é a chave i18n do texto explicativo. No-op se o frame não for
// encontrado.
void install_help_action(brls::Activity* activity, const char* frameId,
                         const std::string& bodyKey);

} // namespace thomaz
