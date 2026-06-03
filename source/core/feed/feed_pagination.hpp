#pragma once
#include <string>
#include <vector>
#include "core/feed/feed_types.hpp"

namespace thomaz::feed {

// Anexa em `acc` apenas os posts de `page` cujo id ainda não está em `acc`
// (dedup por Post::id), preservando a ordem. Retorna page.hasMore.
bool merge_feed_page(std::vector<Post>& acc, const FeedPage& page);

// Ponteiro para o post com esse id dentro de `acc`, ou nullptr. Usado para
// atualizar contadores de curtida/comentário in-place. Válido até `acc` mudar.
Post* find_post(std::vector<Post>& acc, const std::string& id);

} // namespace thomaz::feed
