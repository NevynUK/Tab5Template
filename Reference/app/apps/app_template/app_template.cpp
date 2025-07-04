/**
 * @file app_template.cpp
 * @author Forairaaaaa
 * @brief
 * @version 0.1
 * @date <date></date>
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "app_template.h"
#include <hal/hal.h>
#include <mooncake.h>
#include <mooncake_log.h>

using namespace mooncake;

AppTemplate::AppTemplate()
{
    // Configure App information
    setAppInfo().name = "AppTemplate";
}

void AppTemplate::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");

    // Open self
    open();
}

void AppTemplate::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
}

void AppTemplate::onRunning()
{
    // mclog::tagInfo(getAppInfo().name, "on running");
}

void AppTemplate::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
}
