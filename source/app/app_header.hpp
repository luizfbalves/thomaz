#pragma once

#include <string>

namespace brls { class Activity; }

namespace thomaz {

class IAuthClient;

// Injeta o cliente de autenticação usado pelo chip de login do header (para
// abrir a tela de login). Deve ser chamado uma vez no startup (main).
void set_header_auth_client(IAuthClient* client);

// Mostra o status de login no canto direito do header do AppletFrame, como um
// chip clicável: logado => "@usuario" (toque para sair); deslogado => "Entrar"
// (toque para abrir a tela de login). Chamado em onContentAvailable de cada
// activity, para aparecer globalmente em todas as telas.
void install_header_username(brls::Activity* activity);

// Injeta indicadores de sistema (barra SD, barra NAND, WiFi) no hint_box do
// AppletFrame. Deve ser chamado ANTES de install_header_username para que
// a ordem no hint_box seja [SD][NAND][WiFi][@usuario].
void install_system_status(brls::Activity* activity);

// Registra a ação de ajuda no botão "-" (BUTTON_BACK) do rodapé: abre um
// diálogo explicando como a tela funciona. `frameId` é o id do AppletFrame da
// tela; `bodyKey` é a chave i18n do texto explicativo. No-op se o frame não for
// encontrado.
void install_help_action(brls::Activity* activity, const char* frameId,
                         const std::string& bodyKey);

} // namespace thomaz
