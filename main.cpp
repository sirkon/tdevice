#include <cstdio>
#include <cstdlib>
#include <cerrno>

#include <unistd.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#include <err.h>
#include <string>
#include <sstream>
#include <fstream>
#include <exception>
#include <memory>

#include <pthread.h>
#include <openssl/ssl.h>

#include "confparser.hpp"
#include "confbind.hpp"
#include "runtime.hpp"
#include "background.hpp"
#include "controller.hpp"


using namespace std;

const char *CONFIG_FILE_NAME = "conf/targetdevice.yaml";


class GlobalDataInitializer {
public:
    Config* conf;
    Devices* devices;
    Drivers *drivers;
    NamedSchedule *sched;

    GlobalDataInitializer(const char *config_file_name) {
        FILE *fp = open_conf_fp(config_file_name);
        if(fp == NULL) {
            perror(config_file_name);
            errx(
                EXIT_FAILURE,
                "Cannot open configuration file: %s",
                config_file_name);
        }
        auto_ptr<MapElement> res(dynamic_cast<MapElement*>(raw_conf_parse(fp)));
        conf = config_parse(res.get());

        drivers = new Drivers(conf->drivers());
        devices = new Devices(*drivers, conf->devices());
        sched = new NamedSchedule;

        // Switching off devices
        for(Devices::iterator it = devices->begin();
            it != devices->end(); it++) {
            try {
                SwitcherOff command(it->second);
                command.execute();
            } catch(CommandSetupError e) {
                ; // Do nothing, we are just setting them all off wherever possible
            }
        }

        SSL_library_init();
    };

    ~GlobalDataInitializer() throw() {
        delete conf;
        delete devices;
        delete drivers;
        delete sched;
    }
};


class PidfileOpenError: public std::string {
public:
    PidfileOpenError(std::string msg): std::string(msg) {}
    PidfileOpenError(int code): std::string(strerror(code)) {}
};


class PidfileLockError: public std::string {
public:
    PidfileLockError(std::string msg): std::string(msg) {}
    PidfileLockError(int code): std::string(strerror(code)) {}
};


class PidfileOperator {
private:
    int fd;
    PidfileOperator(int _fd): fd(_fd) {}

public:
    static PidfileOperator *open(const char *filename) {
        int fd = ::open(filename,
                  O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK,
                  S_IRUSR | S_IWUSR);
        if(fd < 0) {
            throw PidfileOpenError(errno);
        }

        int status = flock(fd, LOCK_EX | LOCK_NB);
        if(status) {
            close(fd);
            std::ifstream file(filename);
            std::string buf;
            file >> buf;
            throw PidfileLockError(buf);
        }
        status = pwrite(fd, "-1", 2, 0);
        if(status < 0) {
            throw PidfileLockError(status);
        }

        return new PidfileOperator(fd);
    }

    ~PidfileOperator() throw() {
        if(fd > 0) {
            close(fd);
        }
    }

    void write(int pid) {
        std::stringstream buf;
        buf << pid;
        std::string data = buf.str();
        int status = pwrite(fd, data.c_str(), data.size(), 0);
        if(status < 0) {
            ;
        }
    }
};


int main(int argc, char **argv) {
    char *config_file_name;
    pid_t childpid;

    bool daemonize = false;

    for(int i = 1; i < argc; i++) {
        if(string(argv[i]) == "--daemonize") {
            daemonize = true;
        }
    }

    config_file_name = NULL;
    for(int i = 1; i < argc; i++) {
        if(string(argv[i]) == "--conf") {
            if(i == (argc - 1)) {
                errx(EXIT_FAILURE, "No config file name after --conf");
            }
            config_file_name = argv[i + 1];
        }
    }
    auto_ptr<GlobalDataInitializer> init;
    try {
        auto_ptr<GlobalDataInitializer> _init(
            new GlobalDataInitializer(
                config_file_name?config_file_name:CONFIG_FILE_NAME));
        init = _init;
    } catch(exception &err) {
        cerr << err.what();
        exit(EXIT_FAILURE);
    }

    auto_ptr<PidfileOperator> pidoperator;
    if(daemonize) {
        const char *pidfilename = init->conf->daemon().pidfile.c_str();
        PidfileOperator *pidop;

        try {
            pidop = PidfileOperator::open(pidfilename);
        } catch(PidfileOpenError e) {
            syslog(LOG_ERR, "Error: %s", e.c_str());
            return -1;
        } catch(PidfileLockError e) {
            syslog(
                LOG_ERR,
                "Cannot start daemon since another instance is running: %s",
                e.c_str());
            return -1;
        }

        if(daemon(0, 0) == -1) {
            warn("Cannot daemonize");
            delete pidop;
            exit(EXIT_FAILURE);
        }

        pidop->write(getpid());
        childpid = fork();

        switch(childpid) {
        case -1:
            syslog(LOG_ERR, "Cannot fork(): %s.", strerror(errno));
            return -1;
            break;
        case 0:
            pidoperator = auto_ptr<PidfileOperator>(pidop);
            pidop->write(getpid());
            break;
        default:
            syslog(LOG_INFO, "Child %d started.", (int)childpid);
            return 0;
            break;
        }
    }

    // Do work
    pthread_t thread;
    pthread_create(&thread, NULL, background_worker, init->sched);

    Controller controller(init->conf, init->devices, init->sched);
    while(true) {
        try {
            controller.execute();
        } catch(ConnectionError &e) {
            cout << "Cannot connect: " << e.what() << endl;
        }
        sleep(120);
    }

    return 0;
}
