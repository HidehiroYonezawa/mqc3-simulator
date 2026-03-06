#include "server/common/message_manager.h"

MessageManager* MessageManager::GetMessageManager() {
    static MessageManager MessageManager;
    return &MessageManager;
}
