// Copyright (c) Microsoft Corporation
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "pch.h"
#include <time.h>
#include "Support\Game.h"
#include "Support\PerformanceCounters.h"

using namespace Concurrency;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation;
using namespace xbox::services;
using namespace xbox::services::social::manager;

void
Game::InitializeSocialManager(Windows::Foundation::Collections::IVectorView<Windows::Xbox::System::User^>^ userList)
{
    m_socialManager = social_manager::get_singleton_instance();

    for (Windows::Xbox::System::User^ user : userList)
    {
        AddUserToSocialManager(user);
    }
}

void
Game::AddUserToSocialManager(
    _In_ Windows::Xbox::System::User^ user
    )
{
    std::shared_ptr<xbox::services::xbox_live_context> xboxLiveContext = std::make_shared<xbox::services::xbox_live_context>(user);

    {
        std::lock_guard<std::mutex> guard(m_socialManagerLock);

        stringstream_t source;
        source << _T("Adding user ");
        source << user->DisplayInfo->Gamertag->Data();
        source << _T(" to SocialManager");
        Log(source.str());

        m_socialManager->add_local_user(user, social_manager_extra_detail_level::no_extra_detail);
    }

    CreateSocialGroupsBasedOnUI(user);
}

void
Game::RemoveUserFromSocialManager(
    _In_ Windows::Xbox::System::User^ user
    )
{
    std::lock_guard<std::mutex> guard(m_socialManagerLock);

    stringstream_t source;
    source << _T("Removing user ");
    source << user->DisplayInfo->Gamertag->Data();
    source << _T(" from SocialManager");
    Log(source.str());

    auto it = m_socialGroups.begin();
    while (it != m_socialGroups.end()) 
    {
        std::shared_ptr<xbox::services::social::manager::xbox_social_user_group> group = *it;
        if (wcscmp(group->local_user()->XboxUserId->Data(), user->XboxUserId->Data()))
        {
            it = m_socialGroups.erase(it);
        }
        else 
        {
            ++it;
        }
    }

    m_socialManager->remove_local_user(user);
}

void 
Game::CreateSocialGroupFromList(
    _In_ Windows::Xbox::System::User^ user,
    _In_ std::vector<string_t> xuidList
    )
{
    if( xuidList.size() > 0 )
    {
        auto result = m_socialManager->create_social_user_group_from_list(user, xuidList);
        if (!result.err())
        {
            std::lock_guard<std::mutex> guard(m_socialManagerLock);
            m_socialGroups.push_back(result.payload());
        }
    }
}

void
Game::CreateSocialGroupFromFilters(
    _In_ Windows::Xbox::System::User^ user,
    _In_ presence_filter presenceFilter,
    _In_ relationship_filter relationshipFilter
    )
{
    auto result = m_socialManager->create_social_user_group_from_filters(user, presenceFilter, relationshipFilter);
    if (!result.err())
    {
        std::lock_guard<std::mutex> guard(m_socialManagerLock);
        m_socialGroups.push_back(result.payload());
    }
}

void
Game::DestorySocialGroupFromList(
    _In_ Windows::Xbox::System::User^ user
    )
{
    std::lock_guard<std::mutex> guard(m_socialManagerLock);

    auto it = m_socialGroups.begin();
    while (it != m_socialGroups.end())
    {
        std::shared_ptr<xbox::services::social::manager::xbox_social_user_group> group = *it;
        if (wcscmp(group->local_user()->XboxUserId->Data(), user->XboxUserId->Data()) == 0 &&
            group->social_user_group_type() == social_user_group_type::user_list_type)
        {
            m_socialManager->destroy_social_user_group(group);
            it = m_socialGroups.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void
Game::DestroySocialGroup(
    _In_ Windows::Xbox::System::User^ user,
    _In_ presence_filter presenceFilter,
    _In_ relationship_filter relationshipFilter
    )
{
    std::lock_guard<std::mutex> guard(m_socialManagerLock);

    auto it = m_socialGroups.begin();
    while (it != m_socialGroups.end())
    {
        std::shared_ptr<xbox::services::social::manager::xbox_social_user_group> group = *it;
        if (wcscmp(group->local_user()->XboxUserId->Data(), user->XboxUserId->Data()) == 0 &&
            group->presence_filter_of_group() == presenceFilter &&
            group->relationship_filter_of_group() == relationshipFilter)
        {
            m_socialManager->destroy_social_user_group(group);
            it = m_socialGroups.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void
Game::UpdateSocialManager()
{
    std::lock_guard<std::mutex> guard(m_socialManagerLock);

#if PERF_COUNTERS
    auto perfInstance = performance_counters::get_singleton_instance();
    perfInstance->begin_capture(L"no_updates");
    perfInstance->begin_capture(L"updates");
#endif
    auto socialEvents = m_socialManager->do_work();
#if PERF_COUNTERS
    if (socialEvents.empty())
    {
        perfInstance->end_capture(L"no_updates");
    }
    else
    {
        perfInstance->end_capture(L"updates");
    }
#endif

    LogSocialEventList(socialEvents);
}

std::vector<std::shared_ptr<xbox::services::social::manager::xbox_social_user_group>> Game::GetSocialGroups()
{
    std::lock_guard<std::mutex> guard(m_socialManagerLock);
    return m_socialGroups;
}



