#pragma once

namespace brls { class Activity; }

namespace thomaz {

// Mostra "@usuario" (sessão atual) no canto direito do header do AppletFrame.
// Chamado em onContentAvailable de cada activity, para o nome aparecer
// globalmente em todas as telas. No-op se não houver sessão ou header.
void install_header_username(brls::Activity* activity);

} // namespace thomaz
