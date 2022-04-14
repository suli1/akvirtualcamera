/* akvirtualcamera, virtual camera for Mac and Windows.
 * Copyright (C) 2020  Gonzalo Exequiel Pedone
 *
 * akvirtualcamera is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * akvirtualcamera is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with akvirtualcamera. If not, see <http://www.gnu.org/licenses/>.
 *
 * Web-Site: http://webcamoid.github.io/
 */

#include <cstring>
#include <CoreFoundation/CFRunLoop.h>
#include <xpc/xpc.h>
#include <xpc/connection.h>

#include "assistant.h"
#include "assistantglobals.h"
#include "PlatformUtils/src/preferences.h"
#include "VCamUtils/src/logger.h"

GLOBAL_STATIC(AkVCam::Assistant, assistant)

int main(int argc, char **argv)
{
    auto loglevel = AkVCam::Preferences::logLevel();
    AkVCam::Logger::setLogLevel(AKVCAM_LOGLEVEL_DEBUG);
    AkLogDebug() << "Creating Service: " << CMIO_ASSISTANT_NAME << std::endl;
    auto server =
            xpc_connection_create_mach_service(CMIO_ASSISTANT_NAME,
                                               NULL,
                                               XPC_CONNECTION_MACH_SERVICE_LISTENER);

    if (!server)
        return EXIT_FAILURE;

    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            auto timeout = strtod(argv[i + 1], nullptr);
            AkLogInfo() << "Set timeout: " << timeout << std::endl;
            assistant()->setTimeout(timeout);
        } else if (strcmp(argv[i], "--loglevel") == 0 && i + 1 < argc) {
            auto loglevel = strtoul(argv[i + 1], nullptr, 10);
            AkVCam::Logger::setLogLevel(loglevel);
            AkLogInfo() << "Set loglevel: " << loglevel << std::endl;
        }

    AkLogDebug() << "Setting up handler" << std::endl;

    xpc_connection_set_event_handler(server, ^(xpc_object_t event) {
        auto type = xpc_get_type(event);

        if (type == XPC_TYPE_ERROR) {
             auto description = xpc_copy_description(event);
             AkLogError() << description << std::endl;
             free(description);

             return;
        }

        auto client = reinterpret_cast<xpc_connection_t>(event);

        xpc_connection_set_event_handler(client, ^(xpc_object_t event) {
            assistant()->messageReceived(client, event);
        });

        xpc_connection_resume(client);
    });

    AkLogDebug() << "Resuming connection" << std::endl;
    xpc_connection_resume(server);
    AkLogDebug() << "Running loop" << std::endl;
    CFRunLoopRun();
    xpc_release(server);

    return EXIT_SUCCESS;
}
