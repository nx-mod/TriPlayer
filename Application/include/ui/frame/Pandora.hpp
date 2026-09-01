#ifndef FRAME_PANDORA_HPP
#define FRAME_PANDORA_HPP

#include <future>
#include "ui/frame/Frame.hpp"

namespace Frame {
    // Placeholder frame for Pandora streaming -- station browsing/playback
    // isn't wired up yet. For now this just logs in (using credentials read
    // from pianobar-nx's account.cfg) as a first proof-of-concept step
    // toward full integration.
    class Pandora : public Frame {
        private:
            Aether::Text * status;
            std::future<bool> loginThread;
            bool threadDone;
            std::string resultMsg;

            // Reads user/password from sdmc:/config/pianobar-nx/account.cfg.
            // Returns false if the file is missing or incomplete.
            bool readAccount(std::string & user, std::string & password);

            // Runs on a background thread: does the actual login handshake.
            // Fills resultMsg with a status/error string either way.
            bool doLogin(const std::string & user, const std::string & password);

        public:
            Pandora(Main::Application *);

            void update(uint32_t);
    };
};

#endif
