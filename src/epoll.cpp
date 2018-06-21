#include <sys/epoll.h>
#include <unistd.h>


#include "epoll.hpp"

namespace mongols {

    epoll::epoll(int ev_size, int timeout)
    : epfd(0)
    , ev_size(ev_size)
    , timeout(timeout)
    , ev(), evs(0) {
        this->epfd = epoll_create1(0);
        if (this->epfd > 0) {
            this->evs = (struct epoll_event*) malloc(sizeof (struct epoll_event) * this->ev_size);
        }
    }

    epoll::~epoll() {
        if (this->evs != 0) {
            free(this->evs);
        }
        close(this->epfd);
    }

    bool epoll::is_ready() const {
        return this->epfd > 0 && this->evs != 0;
    }

    bool epoll::add(int fd, int event) {
        this->ev.data.fd = fd;
        this->ev.events = event;
        return epoll_ctl(this->epfd, EPOLL_CTL_ADD, fd, &this->ev) == 0;
    }

    bool epoll::del(int fd) {
        this->ev.data.fd = fd;
        return epoll_ctl(this->epfd, EPOLL_CTL_DEL, fd, &this->ev) == 0;
    }

    bool epoll::mod(int fd, int event) {
        this->ev.data.fd = fd;
        this->ev.events = event;
        return epoll_ctl(this->epfd, EPOLL_CTL_MOD, fd, &this->ev) == 0;
    }

    int epoll::wait() {
        return epoll_wait(this->epfd, this->evs, this->ev_size, this->timeout);
    }

    void epoll::loop(int size, const std::function<void(struct epoll_event*) >& handler) {
        for (int i = 0; i < size; ++i) {
            handler(&this->evs[i]);
        }
    }

    size_t epoll::size() const {
        return (size_t)this->ev_size;
    }

    size_t epoll::expires() const {
        return (size_t)this->timeout;
    }
}
