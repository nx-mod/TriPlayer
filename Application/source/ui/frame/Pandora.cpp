#include <cerrno>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <piano.h>
#include "Application.hpp"
#include "Log.hpp"
#include "ui/frame/Pandora.hpp"

// Same default Pandora partner credentials pianobar itself uses (see
// pianobar-nx's settings.c) -- these identify the *app* to Pandora's API,
// not the user, so they're fine to keep as plain constants here too.
static const char * const partnerUser = "android";
static const char * const partnerPassword = "AC7IBG09A3DTSYM4R41UJWL07VLN8JI7";
static const char * const device = "android-generic";
static const char * const inkey = "R=U!LH$O2B#";
static const char * const outkey = "6#26FRL$ZWD";

static size_t curlWriteCb(char * ptr, size_t size, size_t nmemb, void * userdata) {
    std::string * buf = static_cast<std::string *>(userdata);
    buf->append(ptr, size * nmemb);
    return size * nmemb;
}

// Sends one PianoRequest over HTTPS and feeds the response back into
// libpiano, looping while it asks for another round (used for multi-step
// requests like login). Mirrors pianobar-nx's BarUiPianoCall/
// BarPianoHttpRequest, trimmed down to just what login needs.
static PianoReturn_t doPianoRequest(PianoHandle_t * ph, PianoRequestType_t type, void * data) {
    PianoReturn_t pRet;
    PianoRequest_t req;
    int round = 0;

    do {
        round++;
        std::memset(&req, 0, sizeof(req));
        req.data = data;

        pRet = PianoRequest(ph, &req, type);
        Log::writeError("[PANDORA] round " + std::to_string(round) + " PianoRequest -> " + PianoErrorToStr(pRet) + " (" + std::to_string((int)pRet) + ")");
        if (pRet != PIANO_RET_OK) {
            return pRet;
        }

        std::string url = std::string(req.secure ? "https://" : "http://") + PIANO_RPC_HOST + req.urlPath;
        Log::writeError("[PANDORA] round " + std::to_string(round) + " POST " + url);
        std::string response;

        CURL * curl = curl_easy_init();
        struct curl_slist * headers = curl_slist_append(nullptr, "Content-Type: text/plain");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.postData);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "TriPlayer-Pandora");
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        CURLcode cRet = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);

        Log::writeError("[PANDORA] round " + std::to_string(round) + " curl -> " +
                std::string(curl_easy_strerror(cRet)) + " http=" + std::to_string(httpCode) +
                " bytes=" + std::to_string(response.size()));

        if (cRet != CURLE_OK) {
            PianoDestroyRequest(&req);
            return PIANO_RET_ERR;
        }

        // log a snippet of the response body -- helps distinguish "got an
        // error JSON back from Pandora" vs "got nothing/garbage"
        Log::writeError("[PANDORA] round " + std::to_string(round) + " body: " +
                response.substr(0, 200));

        req.responseData = strdup(response.c_str());
        pRet = PianoResponse(ph, &req);
        // PianoErrorToStr() has a hard assert(0) for PIANO_RET_CONTINUE_REQUEST
        // (upstream never expects it to be stringified -- it's the internal
        // "loop again" signal, not a terminal result) -- calling it
        // unconditionally here is exactly what took the whole console down
        // earlier tonight.
        Log::writeError("[PANDORA] round " + std::to_string(round) + " PianoResponse -> " +
                (pRet == PIANO_RET_CONTINUE_REQUEST ? "CONTINUE_REQUEST" : PianoErrorToStr(pRet)) +
                " (" + std::to_string((int)pRet) + ")");
        PianoDestroyRequest(&req);
    } while (pRet == PIANO_RET_CONTINUE_REQUEST);

    return pRet;
}

namespace Frame {
    bool Pandora::readAccount(std::string & user, std::string & password) {
        FILE * f = std::fopen("sdmc:/config/pianobar-nx/account.cfg", "r");
        if (f == nullptr) {
            Log::writeError("[PANDORA] fopen account.cfg failed: " + std::string(std::strerror(errno)));
            return false;
        }

        char line[256];
        while (std::fgets(line, sizeof(line), f) != nullptr) {
            char * eq = std::strchr(line, '=');
            if (eq == nullptr) {
                continue;
            }
            *eq = '\0';
            char * key = line;
            char * val = eq + 1;

            // trim
            while (*key == ' ') key++;
            char * keyEnd = key + std::strlen(key);
            while (keyEnd > key && (keyEnd[-1] == ' ')) *(--keyEnd) = '\0';
            while (*val == ' ') val++;
            char * valEnd = val + std::strlen(val);
            while (valEnd > val && (valEnd[-1] == '\n' || valEnd[-1] == '\r' || valEnd[-1] == ' ')) *(--valEnd) = '\0';

            if (std::strcmp(key, "user") == 0) {
                user = val;
            } else if (std::strcmp(key, "password") == 0) {
                password = val;
            }
        }
        std::fclose(f);

        bool ok = (!user.empty() && !password.empty());
        Log::writeError("[PANDORA] readAccount: user=" + (user.empty() ? "<empty>" : user) +
                " password=" + (password.empty() ? "<empty>" : "<set, " + std::to_string(password.size()) + " chars>"));
        return ok;
    }

    bool Pandora::doLogin(const std::string & user, const std::string & password) {
        Log::writeError("[PANDORA] doLogin starting for " + user);

        PianoHandle_t ph;
        PianoReturn_t pRet = PianoInit(&ph, partnerUser, partnerPassword, device, inkey, outkey);
        Log::writeError("[PANDORA] PianoInit -> " + std::string(PianoErrorToStr(pRet)));
        if (pRet != PIANO_RET_OK) {
            this->resultMsg = "Init failed: " + std::string(PianoErrorToStr(pRet));
            return false;
        }

        PianoRequestDataLogin_t loginData;
        loginData.user = const_cast<char *>(user.c_str());
        loginData.password = const_cast<char *>(password.c_str());
        loginData.step = 0;

        pRet = doPianoRequest(&ph, PIANO_REQUEST_LOGIN, &loginData);
        if (pRet != PIANO_RET_OK) {
            this->resultMsg = "Login failed: " + std::string(PianoErrorToStr(pRet)) + " (" + std::to_string((int)pRet) + ")";
            Log::writeError("[PANDORA] " + this->resultMsg);
            PianoDestroy(&ph);
            return false;
        }

        this->resultMsg = "Logged in as " + user;
        Log::writeSuccess("[PANDORA] " + this->resultMsg);
        // NOTE: ph is intentionally leaked here (not PianoDestroy'd) --
        // this is a login-only proof of concept, nothing downstream uses
        // the authenticated handle yet. Revisit once station browsing/
        // playback is wired up.
        return true;
    }

    Pandora::Pandora(Main::Application * a) : Frame(a) {
        this->heading->setString("Pandora");
        this->heading->setColour(this->app->theme()->FG());

        this->sort->setHidden(true);
        this->titleH->setHidden(true);
        this->artistH->setHidden(true);
        this->albumH->setHidden(true);
        this->lengthH->setHidden(true);
        this->topContainer->setHasSelectable(false);
        this->list->setHasSelectable(false);

        this->status = new Aether::Text(this->x() + 65, this->y() + 240, "Logging in...", 24);
        this->status->setColour(this->app->theme()->muted());
        this->addElement(this->status);

        // Run synchronously on this (the main/render) thread, matching
        // pianobar-nx's own architecture exactly -- it never runs its
        // libpiano/curl/json-c calls off the main thread either. This
        // blocks the UI for the ~1s a login takes, which is an acceptable
        // tradeoff for a proof of concept; running the same call chain via
        // std::async on a background thread reliably took the whole
        // console down (fatal 2349-0004, zeroed registers/backtrace --
        // consistent with heap corruption from concurrent allocation
        // rather than a clean crash) even though Search.cpp's own
        // std::async pattern is fine for local DB work. Revisit moving
        // this back to a background thread only with real evidence of
        // what's actually unsafe about calling curl/json-c/mbedtls
        // concurrently with the render thread's own allocations here.
        std::string user, password;
        if (!this->readAccount(user, password)) {
            this->resultMsg = "No saved Pandora account found (sdmc:/config/pianobar-nx/account.cfg)";
            Log::writeError("[PANDORA] " + this->resultMsg);
        } else {
            this->doLogin(user, password);
        }
        this->status->setString(this->resultMsg);
    }
};
