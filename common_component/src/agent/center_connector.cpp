/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: eyjian@qq.com or eyjian@gmail.com
 */
#include <sys/close_helper.h>
#include "agent_context.h"
#include "center_connector.h"
MOOON_NAMESPACE_BEGIN

CCenterConnector::CCenterConnector(CReportQueue* report_queue)
    :_report_queue(report_queue)
{    
}

void CCenterConnector::send_heartbeat()
{
}

net::epoll_event_t CCenterConnector::send_report()
{    
    report_message_t* report_message;
    sys::CLockHelper<sys::CLock> lock_helper(_lock);

    while (_report_queue->pop_front(report_message))
    {
        try
        {
            _center_connector.full_send(report_message->date, report_message->data_size);
            delete [](char*)report_message;
        }
        catch (sys::CSyscallException& ex)
        {
            delete [](char*)report_message;
            return net::epoll_close;
        }
    }
}

net::epoll_event_t CCenterConnector::handle_epoll_event(void* ptr, uint32_t events)
{          
    net::epoll_event_t retval;
    
    if (events & EPOLLIN)
        retval = handle_epoll_read(ptr);
    else if (events & EPOLLOUT)
        retval = handle_epoll_write(ptr);
    else
        retval = handle_epoll_error(ptr);
    
    return retval;
}

net::epoll_event_t CCenterConnector::handle_epoll_read(void* ptr)
{
    agent_message_header_t header;
    size_t header_size = sizeof(header);

    try
    {    
        // 接收包头
        if (!full_receive(&header, header_size))
        {
            AGENT_LOG_ERROR("Connect closed by peer %s:%d.\n", get_peer_ip().to_string().c_str(), get_peer_port());
            return net::epoll_close;
        }

        // 接收包体
        size_t body_size = header.body_length;
        char* body = new char[body_size];
        util::delete_helper<char> dh(body, true);
        if (!full_receive(body, body_size))
        {
            AGENT_LOG_ERROR("Connect closed by peer %s:%d.\n", get_peer_ip().to_string().c_str(), get_peer_port());
            return net::epoll_close;
        }
    }
    catch (sys::CSyscallException& ex)
    {
        AGENT_LOG_ERROR("Agent receive error: %s", sys::CSysUtil::get_error_message(ex.get_errcode()).c_str());
        return net::epoll_close;
    }

    _thread->process_command(&header, body, body_size);
    return net::epoll_none;
}

net::epoll_event_t CCenterConnector::handle_epoll_write(void* ptr)
{
    return send_report();
}

net::epoll_event_t CCenterConnector::handle_epoll_error(void* ptr)
{
    return net::epoll_close;
}

MOOON_NAMESPACE_END