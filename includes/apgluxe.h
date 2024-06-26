#ifdef _WIN32
#include <winsock.h>
#endif

#include <atomic>
#include <thread>

#ifdef _POSIX_SOURCE
void sigwaiter(const sigset_t *set, std::atomic<bool> *running)
{
    int s, sig;

    s = sigwait(set, &sig);
    if (s != 0)
        handle_error_en(s, "sigwait");
    std::cout << "Got signal " << sig << ", will attempt to submit results before exiting" << std::endl;
    *running = false;
}
#endif

int run_apgluxe(int argc, char *argv[]) {

    if (apg::rule2int(RULESTRING) != 0) {
        std::cerr << "Abort: apgsearch rule does not match lifelib rule" << std::endl;
        return 1;
    }

    #ifdef _WIN32
    WSAData wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);
    #endif

    // Default values:
    #ifdef USING_GPU
    int gpuCount = apg::getDeviceCount();

    if (gpuCount <= 0) {
        std::cerr << "Abort: apgsearch has not detected any GPUs" << std::endl;
        return 1;
    } else {
        std::cerr << "Info: apgsearch has successfully detected " << gpuCount << " GPUs" << std::endl;
    }

    int64_t soupsPerHaul = 200000000;
    #else
    int64_t soupsPerHaul = 10000000;
    #endif

    std::string payoshaKey = "#anon";
    std::string seed = reseed("original seed");
    int numThreads = 0;

    #ifdef STDIN_SYM
    int local_log = 1;
    int verifications = 0;
    soupsPerHaul = 100000;
    #else
    int local_log = 0;
    int verifications = -1;
    #endif

    bool testing = false;
    int nullargs = 1;
    bool quitByUser = false;

    int iterations = 0;

    bool consumed_rule = false;
    bool consumed_sym = false;
    
    int unicount = 8192;

    // Extract options:
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], "-k") == 0) {
            payoshaKey = argv[i+1];
        } else if (strcmp(argv[i], "-s") == 0) {
            seed = argv[i+1];
        } else if (strcmp(argv[i], "-n") == 0) {
            soupsPerHaul = atoll(argv[i+1]);
        } else if (strcmp(argv[i], "-v") == 0) {
            verifications = atoi(argv[i+1]);
        } else if (strcmp(argv[i], "-i") == 0) {
            iterations = atoi(argv[i+1]);
        } else if (strcmp(argv[i], "-L") == 0) {
            local_log = atoi(argv[i+1]);
        } else if (strcmp(argv[i], "-u") == 0) {
            unicount = atoi(argv[i+1]);
        } else if (strcmp(argv[i], "-t") == 0) {
            testing = atoi(argv[i+1]);
            if (testing) { iterations = 1; }
        } else if (strcmp(argv[i], "-p") == 0) {
            numThreads = atoi(argv[i+1]);
        } else if (strcmp(argv[i], "--rule") == 0) {
            if (!consumed_rule) {
                std::cout << "\033[1;33mapgluxe " << APG_VERSION << "\033[0m: ";
                std::string desired_rulestring = argv[i+1];
                if (strcmp(RULESTRING, argv[i+1]) == 0) {
                    std::cout << "Rule \033[1;34m" << RULESTRING << "\033[0m is correctly configured." << std::endl;
                } else {
                    std::cout << "Rule \033[1;34m" << RULESTRING << "\033[0m does not match desired rule \033[1;34m";
                    std::cout << desired_rulestring << "\033[0m." << std::endl;
                    execvp("./immediate.sh", argv);
                    return 1;
                }
            }
            nullargs += 1;
            consumed_rule = true;
        } else if (strcmp(argv[i], "--symmetry") == 0) {
            if (!consumed_sym) {
                std::cout << "\033[1;33mapgluxe " << APG_VERSION << "\033[0m: ";
                std::string desired_symmetry = argv[i+1];
                if (strcmp(SYMMETRY2, argv[i+1]) == 0) {
                    std::cout << "Symmetry \033[1;34m" << SYMMETRY2 << "\033[0m is correctly configured." << std::endl;
                } else {
                    std::cout << "Symmetry \033[1;34m" << SYMMETRY2 << "\033[0m does not match desired symmetry \033[1;34m";
                    std::cout << desired_symmetry << "\033[0m." << std::endl;
                    execvp("./immediate.sh", argv);
                    return 1;
                }
            }
            nullargs += 1;
            consumed_sym = true;
        }
    }

    for (int i = 1; i < argc; i++) {
        if ((argv[i][0] == '-') && (argv[i][1] == '-')) { nullargs += 1; }
    }

    if ((argc == nullargs) && (argc > 1)) { return 0; }

    if (argc == 1) {
        std::cout << "Please enter number of soups per haul (minimum 10000000): ";
        std::cin >> soupsPerHaul;
        if (soupsPerHaul < 10000000) { soupsPerHaul = 10000000; }
        std::cout << "Please enter payosha256 key (e.g. '#anon'): ";
        std::cin >> payoshaKey;
        while ((payoshaKey.substr(0, 1) == "'") || (payoshaKey[0] == '"')) {
            payoshaKey = payoshaKey.substr(1);
            payoshaKey = payoshaKey.substr(0, payoshaKey.length() - 1);
        }
        #ifndef __CYGWIN__
        std::cout << "Please enter number of CPU threads to use (e.g. 4): ";
        std::cin >> numThreads;
        #endif
    }

    #ifdef __CYGWIN__
    if (numThreads > 0) {
        std::cout << "Warning: parallelisation disabled on Cygwin." << std::endl;
        numThreads = 0;
    }
    #endif

    #ifdef STDIN_SYM
    if (numThreads > 0) {
        std::cout << "Warning: parallelisation disabled for stdin searching." << std::endl;
        numThreads = 0;
    }
    #endif

    // Disable verification by default if running on a HPC;
    // otherwise verify five hauls per uploaded haul:
    if (verifications < 0) {
        verifications = (numThreads <= 4) ? 5 : 0;
    }
    
    std::cout << "\nGreetings, this is \033[1;33mapgluxe " << APG_VERSION;
    std::cout << "\033[0m, configured for \033[1;34m" << RULESTRING << "/";
    std::cout << SYMMETRY2 << "\033[0m.\n" << std::endl;

    std::cout << "\033[32;1mLifelib version:\033[0m " << LIFELIB_VERSION << std::endl;
    std::cout << "\033[32;1mCompiler version:\033[0m " << __VERSION__ << std::endl;
    std::cout << "\033[32;1mPython version:\033[0m " << PYTHON_VERSION << std::endl;

    std::cout << std::endl;

    if ((unicount != 4096) && (unicount != 8192)) {
        std::cout << "Universe count, " << unicount << ", must be 4096 or 8192." << std::endl;
        return 1;
    }

    if (soupsPerHaul <= 0) {
        std::cout << "Soups per haul, " << soupsPerHaul << ", must be positive." << std::endl;
        return 1;
    }

    if (soupsPerHaul > 10000000000ll) {
        std::cout << "Soups per haul reduced to maximum of 10^10" << std::endl;
        soupsPerHaul = 10000000000ll;
    }

    std::atomic<bool> running(true);

    #ifdef STDIN_SYM
    bool interactive = false;
    #else
    bool interactive = (numThreads == 0);
    #endif

    #ifdef USING_GPU
    if (soupsPerHaul % 1000000) {
        soupsPerHaul -= (soupsPerHaul % 1000000);
        soupsPerHaul += 1000000;
    }
    if (numThreads == 0) { numThreads = 8; }
    #endif

    #ifdef _POSIX_SOURCE
    if (!interactive) {
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGHUP);
        sigaddset(&set, SIGINT);
        sigaddset(&set, SIGTERM);
        pthread_sigmask(SIG_BLOCK, &set, NULL);
        std::thread waiter = std::thread(sigwaiter, &set, &running);
        waiter.detach();
    }
    #endif

    while (!quitByUser) {
        if (verifications > 0) {
            std::cout << "Peer-reviewing hauls:\n" << std::endl;
            // Verify some hauls:
            for (int j = 0; j < verifications; j++) {
                bool earlyquit = verifySearch(payoshaKey);
                if (earlyquit) { break; }
            }
            std::cout << "\nPeer-review complete; proceeding search.\n" << std::endl;
        }

        // Run the search:
        std::cout << "Using seed " << seed << std::endl;

        perpetualSearch(soupsPerHaul, numThreads, interactive, payoshaKey, seed, unicount, local_log, running, testing);

        quitByUser = ! running;

        seed = reseed(seed);
        std::cout << "New seed: " << seed << "; iterations = " << iterations << "; quitByUser = " << quitByUser << std::endl;

        iterations -= 1;
        if (iterations == 0) { break; }
    }

    std::cout << "Terminating..." << std::endl;

    return 0;
}
