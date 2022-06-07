// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
#include <csignal>
#endif
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>

#include <vsomeip/vsomeip.hpp>

#include "sample-ids.hpp"

class service_sample {
public:
    service_sample(bool _use_static_routing) :
            app_(vsomeip::runtime::get()->create_application()),
            is_registered_(false),
            use_static_routing_(_use_static_routing),
            blocked_(false),
            running_(true),
            offer_thread_(std::bind(&service_sample::run, this)) {
    }

    bool init() {
        std::lock_guard<std::mutex> its_lock(mutex_);

        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }
        app_->register_state_handler(
                std::bind(&service_sample::on_state, this,
                        std::placeholders::_1));
        app_->register_message_handler(
                SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_METHOD_ID,
                std::bind(&service_sample::on_message, this,
                        std::placeholders::_1));

        std::cout << "Static routing " << (use_static_routing_ ? "ON" : "OFF")
                  << std::endl;
        return true;
    }

    void start() {
        app_->start();
    }

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    /*
     * Handle signal to shutdown
     */
    void stop() {
        running_ = false;
        blocked_ = true;
        app_->clear_all_handler();
        stop_offer();
        condition_.notify_one();
        offer_thread_.join();
        app_->stop();
    }
#endif

    void offer() {
        app_->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        app_->offer_service(SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID);
    }

    void stop_offer() {
        app_->stop_offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        app_->stop_offer_service(SAMPLE_SERVICE_ID + 1, SAMPLE_INSTANCE_ID);
    }

    void on_state(vsomeip::state_type_e _state) {
        std::cout << "Application " << app_->get_name() << " is "
                << (_state == vsomeip::state_type_e::ST_REGISTERED ?
                        "registered." : "deregistered.")
                << std::endl;

        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            if (!is_registered_) {
                is_registered_ = true;
                blocked_ = true;
                condition_.notify_one();
            }
        } else {
            is_registered_ = false;
        }
    }

    void on_message(const std::shared_ptr<vsomeip::message> &_request) {
        std::cout << "Received a question from Client/Session [" << std::setw(4)
            << std::setfill('0') << std::hex << _request->get_client() << "/"
            << std::setw(4) << std::setfill('0') << std::hex
            << _request->get_session() << "]"
            << std::endl;

        std::shared_ptr<vsomeip::message> its_response
            = vsomeip::runtime::get()->create_response(_request);
        std::shared_ptr<vsomeip::payload> its_payload
            = vsomeip::runtime::get()->create_payload();
        std::vector<vsomeip::byte_t>  its_payload_data;
        vsomeip::byte_t  req_data_size; 
        req_data_size = _request->get_payload()->get_length();
        vsomeip::byte_t q_[req_data_size];
        std::cout << "The question is : ";
        for(int j = 0 ; j < req_data_size ; j++)
        {
            std::cout <<   _request->get_payload()->get_data()[j];
            q_[j] = _request->get_payload()->get_data()[j];
        }
        std::cout << std::endl;
        // n1(Number 1)  n2(Number 2)  ans(answer)  op( operator position)  Mo(Modulo)
        int n1, n2, ans, op, Mo = 0;
        // char to int
        int ci[req_data_size];
        for(int l = 0 ; l < req_data_size ; l++)
        {
            if(isalnum(q_[l]) == false)
                op = l;
        }
        for(int m = 0 ; m < req_data_size ; m++)
        {
            ci[m] = q_[m] - '0';
        }
        n1 = ci[0];
        for(int n = 0 ; n < op-1 ; n++)
        {
            n1 = n1*10 + ci[n+1];
        }
        n2=ci[op+1];
        for(int o = op+1 ; o < req_data_size-1 ; o++){
            n2=n2*10+ci[o+1];
        }
        if(q_[op] == '+')
        {
            ans = n1 + n2;
        }
        else if(q_[op] == '-')
        {
            ans = n1 - n2;
        }
        else if(q_[op] == '*')
        {
            ans = n1 * n2;
        }
        else if(q_[op] == '/')
        {
            if(n1%n2 == 0)
            {
                ans = n1 / n2;
            }
            else 
            {
                ans = n1/n2;
                Mo = n1%n2;
            }
        }

        vsomeip::byte_t dig = 0, sM;
        std::string s = std::to_string(ans);
        sM = Mo + '0';
        dig = s.length();
        for(int p = 0 ; p < dig ; p++)
        {
            its_payload_data.push_back(s[p]);
        }
        its_payload_data.push_back(sM);
        its_payload->set_data(its_payload_data);
        its_response->set_payload(its_payload);

        app_->send(its_response);
    }

    void run() {
        std::unique_lock<std::mutex> its_lock(mutex_);
        while (!blocked_)
            condition_.wait(its_lock);

        bool is_offer(true);

        if (use_static_routing_) {
            offer();
            while (running_);
        } else {
            while (running_) {
                if (is_offer)
                    offer();
                else
                    stop_offer();

                for (int i = 0; i < 10 && running_; i++)
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                // is_offer = !is_offer;
            }
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    bool is_registered_;
    bool use_static_routing_;

    std::mutex mutex_;
    std::condition_variable condition_;
    bool blocked_;
    bool running_;

    // blocked_ must be initialized before the thread is started.
    std::thread offer_thread_;
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    service_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_static_routing(false);

    std::string static_routing_enable("--static-routing");

    for (int i = 1; i < argc; i++) {
        if (static_routing_enable == argv[i]) {
            use_static_routing = true;
        }
    }

    service_sample its_sample(use_static_routing);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_sample_ptr = &its_sample;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    if (its_sample.init()) {
        its_sample.start();
        return 0;
    } else {
        return 1;
    }
}
