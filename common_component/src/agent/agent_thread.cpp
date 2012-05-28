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
#include ""
AGENT_NAMESPACE_BEGIN

CAgentThread::CAgentThread(CAgentContext* context, uint32_t queue_size)
 :_context(context)
 ,_report_queue(queue_size)
{
}

void CAgentThread::report(const agent_message_header_t* header)
{
    sys::LockHelper<sys::CLock> lh(_queue_lock);
    _report_queue.push_back(header);
}

void CAgentThread::run()
{
    while (!is_stop())
    {
        try
        {
            uint32_t milliseconds = 2000;
            int num = _epoller.timed_wait(milliseconds);
            if (0 == num)
            {
                // timeout
            }
            else
            {
                for (int i=0; i<num; ++i)
                {
                    uint32_t events = get_events(i);
                    CEpollable* epollable = _epoller.get(i);                    
                    net::epoll_event_t ee = epollable->handle_epoll_event(NULL, events, NULL);
                    switch (ee)
                    {
                    case net::epoll_read
                        _epoller.set_events(epollable, EPOLLIN);
                        break;
                    case net::epoll_write
                        _epoller.set_events(epollable, EPOLLOUT);
                        break;
                    case net::epoll_read_write
                        _epoller.set_events(epollable, EPOLLIN | EPOLLOUT);
                        break;
                    case net::epoll_remove:
                        _epoller.del_events(epollable);
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        catch (sys::CSyscallException& ex)
        {
            AGENT_LOG_ERROR("%s.\n", ex.to_string().c_str());
        }
    }
    
    _epoller.destroy();
}

bool CAgentThread::before_start()
{
    _epoller.create(1024);
    _epoller.set_events(&_report_queue, EPOLLIN);
    
    return true;
}

AGENT_NAMESPACE_END
