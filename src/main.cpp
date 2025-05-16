#include "utils/logger.h"
#include <coreinit/filesystem.h>
#include <malloc.h>
#include <wups.h>
#include <wups/config/WUPSConfigItemBoolean.h>
#include <wups/config/WUPSConfigItemIntegerRange.h>
#include <wups/config/WUPSConfigItemMultipleValues.h>
#include <wups/config/WUPSConfigItemStub.h>
#include <wups/config_api.h>
#include <string>
#include <bitset>
#include <notifications/notifications.h>
#include <sys/socket.h>
#include <thread>
#include <arpa/inet.h>
#include <nn/ac.h>
#include <unistd.h>
#include <coreinit/memorymap.h>
#include <coreinit/title.h>

/**
    Mandatory plugin information.
    If not set correctly, the loader will refuse to use the plugin.
**/
WUPS_PLUGIN_NAME("Data Viewer Server");
WUPS_PLUGIN_DESCRIPTION("Opens a socket to listen to your controller's inputs");
WUPS_PLUGIN_VERSION("Beta V1");
WUPS_PLUGIN_AUTHOR("Teotia444");
WUPS_PLUGIN_LICENSE("BSD");


#define ENABLE_NIS "enableNIS"
#define STAGE_ID     ((uint8_t*) 0x109763f0)  // 8-byte array
#define SPAWN_ID     ((uint8_t*) 0x109763f9)  // 1 byte
#define ROOM_ID      ((uint8_t*) 0x109763fa)  // 1 byte
#define STAGE_LAYER  ((uint8_t*) 0x109763fb)  // 1 byte

std::string gamepad_hold;
VPADVec2D gamepad_lstick;
VPADVec2D gamepad_rstick;

std::string procon_hold;
VPADVec2D procon_lstick;
VPADVec2D procon_rstick;

bool threadRunning = false;

/**
    All of this defines can be used in ANY file.
    It's possible to split it up into multiple files.

**/

WUPS_USE_WUT_DEVOPTAB();                  // Use the wut devoptabs
WUPS_USE_STORAGE("network_input_server"); // Unique id for the storage api

#define ENABLE_NIS_DEFAULT_VALUE     true
bool enableNIS                     = ENABLE_NIS_DEFAULT_VALUE;

#define STAGE_ID     ((uint8_t*) 0x109763f0)  // 8-byte array
#define SPAWN_ID     ((uint8_t*) 0x109763f9)  // 1 byte
#define ROOM_ID      ((uint8_t*) 0x109763fa)  // 1 byte
#define STAGE_LAYER  ((uint8_t*) 0x109763fb)  // 1 byte

bool isWWHDRunning()
{
    uint64_t valid_ids[] = {
        0x0005000010143600, // EUR
        0x0005000010143500, // USA
        0x0005000010143400, // JPN
        0x0005000010143599  // Randomizer
    };

    uint64_t title_id = OSGetTitleID();

    for (auto id : valid_ids) {
        if (title_id == id) return true;
    }

    return false;
}

uint32_t remapProButtons(uint32_t buttons)
{
    uint32_t conv_buttons = 0;

    if (buttons & WPAD_PRO_BUTTON_LEFT)
    {
        conv_buttons |= VPAD_BUTTON_LEFT;
    }
    if (buttons & WPAD_PRO_BUTTON_RIGHT)
    {
        conv_buttons |= VPAD_BUTTON_RIGHT;
    }
    if (buttons & WPAD_PRO_BUTTON_DOWN)
    {
        conv_buttons |= VPAD_BUTTON_DOWN;
    }
    if (buttons & WPAD_PRO_BUTTON_UP)
    {
        conv_buttons |= VPAD_BUTTON_UP;
    }
    if (buttons & WPAD_PRO_BUTTON_PLUS)
    {
        conv_buttons |= VPAD_BUTTON_PLUS;
    }
    if (buttons & WPAD_PRO_BUTTON_X)
    {
        conv_buttons |= VPAD_BUTTON_X;
    }
    if (buttons & WPAD_PRO_BUTTON_Y)
    {
        conv_buttons |= VPAD_BUTTON_Y;
    }
    if (buttons & WPAD_PRO_BUTTON_B)
    {
        conv_buttons |= VPAD_BUTTON_B;
    }
    if (buttons & WPAD_PRO_BUTTON_A)
    {
        conv_buttons |= VPAD_BUTTON_A;
    }
    if (buttons & WPAD_PRO_BUTTON_MINUS)
    {
        conv_buttons |= VPAD_BUTTON_MINUS;
    }
    if (buttons & WPAD_PRO_BUTTON_HOME)
    {
        conv_buttons |= VPAD_BUTTON_HOME;
    }
    if (buttons & WPAD_PRO_TRIGGER_ZR)
    {
        conv_buttons |= VPAD_BUTTON_ZR;
    }
    if (buttons & WPAD_PRO_TRIGGER_ZL)
    {
        conv_buttons |= VPAD_BUTTON_ZL;
    }
    if (buttons & WPAD_PRO_TRIGGER_R)
    {
        conv_buttons |= VPAD_BUTTON_R;
    }
    if (buttons & WPAD_PRO_TRIGGER_L)
    {
        conv_buttons |= VPAD_BUTTON_L;
    }
    if (buttons & WPAD_PRO_BUTTON_STICK_L)
    {
        conv_buttons |= VPAD_BUTTON_STICK_L;
    }
    if (buttons & WPAD_PRO_BUTTON_STICK_R)
    {
        conv_buttons |= VPAD_BUTTON_STICK_R;
    }
    return conv_buttons;
}

std::string formatInput(std::string rawInputs, VPADVec2D lstick, VPADVec2D rstick)
{
    std::string remap = "0000000000000000";
    std::string payload = "{";

    for (size_t key = 0; key < rawInputs.length(); ++key)
    {
        switch (key)
        {
        case 0: remap[4] = rawInputs[key]; continue;
        case 1: remap[5] = rawInputs[key]; continue;
        case 3: remap[0] = rawInputs[key]; continue;
        case 4: remap[1] = rawInputs[key]; continue;
        case 5: remap[2] = rawInputs[key]; continue;
        case 6: remap[3] = rawInputs[key]; continue;
        case 7: remap[12] = rawInputs[key]; continue;
        case 8: remap[14] = rawInputs[key]; continue;
        case 9: remap[13] = rawInputs[key]; continue;
        case 10: remap[15] = rawInputs[key]; continue;
        case 11: remap[8] = rawInputs[key]; continue;
        case 12: remap[9] = rawInputs[key]; continue;
        case 13: remap[6] = rawInputs[key]; continue;
        case 14: remap[7] = rawInputs[key]; continue;
        case 15: remap[10] = rawInputs[key]; continue;
        case 16: remap[11] = rawInputs[key]; continue;
        default: continue;
        }
    }

    payload += "\"axes\": [" + std::to_string(lstick.x) + "," + std::to_string(-lstick.y) + "," +
               std::to_string(rstick.x) + "," + std::to_string(-rstick.y) + "],";

    payload += "\"buttons\": [";
    for (int key = 0; key < 16; key++)
    {
        std::string read = (remap[key] == '1') ? "true" : "false";
        std::string val = {remap[key]};
        payload += "{\"pressed\": " + read + ", \"value\": " + val + "},";
    }
    payload.pop_back();
    payload += "],";

    if (!isWWHDRunning()) {
        payload += "\"connected\": false, \"error\": \"WWHD not running\"}";
        payload.insert(0, std::to_string(payload.size()) + "#");
        return payload;
    }

    float speed = 0.0f;
    float potential_speed = 0.0f;
    float* speed_ptr = nullptr;
    float* potential_speed_ptr = nullptr;
    float** base = reinterpret_cast<float**>(0x10989C74);
    if (OSIsAddressValid((uint32_t)base)) {
        uint8_t* base_ptr = reinterpret_cast<uint8_t*>(*base);
        if (OSIsAddressValid((uint32_t)base_ptr)) {
            speed_ptr = reinterpret_cast<float*>(base_ptr + 0x6938);
            potential_speed_ptr = reinterpret_cast<float*>(base_ptr + 0x294);
            if (OSIsAddressValid((uint32_t)speed_ptr)) speed = *speed_ptr;
            if (OSIsAddressValid((uint32_t)potential_speed_ptr)) potential_speed = *potential_speed_ptr;
        }
    }

    float* pos_x = reinterpret_cast<float*>(0x1096ef48);
    float* pos_y = reinterpret_cast<float*>(0x1096ef50);
    float* pos_z = reinterpret_cast<float*>(0x1096ef4c);
    uint16_t* facing_angle = reinterpret_cast<uint16_t*>(0x1096ef12);
    uint16_t* speed_angle = reinterpret_cast<uint16_t*>(0x1096ef0a);

    payload += "\"speed\": " + std::to_string(speed) + ",";
    payload += "\"potentialSpeed\": " + std::to_string(potential_speed) + ",";

    if (OSIsAddressValid((uint32_t)pos_x) && OSIsAddressValid((uint32_t)pos_y) &&
        OSIsAddressValid((uint32_t)pos_z)) {
        payload += "\"position\": {\"x\": " + std::to_string(*pos_x) + ", \"y\": " +
                   std::to_string(*pos_y) + ", \"z\": " + std::to_string(*pos_z) + "},";
    }

    if (OSIsAddressValid((uint32_t)facing_angle)) {
        payload += "\"facing\": " + std::to_string(*facing_angle) + ",";
    }
    if (OSIsAddressValid((uint32_t)speed_angle)) {
        payload += "\"speedAngle\": " + std::to_string(*speed_angle) + ",";
    }

    if (OSIsAddressValid((uint32_t)STAGE_ID)) {
        char stage[9];
        memcpy(stage, STAGE_ID, 8);
        stage[8] = '\0';
        payload += "\"stage\": \"" + std::string(stage) + "\",";
    }
    if (OSIsAddressValid((uint32_t)SPAWN_ID)) {
        payload += "\"spawn\": " + std::to_string(*SPAWN_ID) + ",";
    }
    if (OSIsAddressValid((uint32_t)ROOM_ID)) {
        payload += "\"room\": " + std::to_string(*ROOM_ID) + ",";
    }
    if (OSIsAddressValid((uint32_t)STAGE_LAYER)) {
        payload += "\"layer\": " + std::to_string(*STAGE_LAYER) + ",";
    }

    payload += "\"connected\": true, \"id\": \"Wii U Console\", \"index\": 0, \"mapping\": \"standard\", \"timestamp\": 0.0}";
    payload.insert(0, std::to_string(payload.size()) + "#");
    return payload;
}



void socket_thread()
{
        int server_fd, client_fd;
        struct sockaddr_in server_addr;

        // Create a TCP socket
        server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

        // Configure server address structure
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(56710);

        // Bind the socket to the port
        bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));

        // Listen for incoming connections
        listen(server_fd, 5);
        // Accept a client connection
        client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd == -1){
            close(client_fd);
            close(server_fd);
            threadRunning = false;
            return;
        }
        auto buffer = new char[26];
        std::string payload;
        NotificationModule_AddInfoNotification("Input Viewer Connected!");
        while (1)
        {
            auto len = recv(client_fd, buffer, 26, 0);
            char str[256] = "";
            snprintf(str, sizeof str, "%zu", len);
            if (len < 1)
            {
                close(client_fd);
                break;
            }

            std::string hold;
            VPADVec2D lstick;
            VPADVec2D rstick;

            if (procon_hold != std::bitset<19>(0).to_string())
            {
                hold = procon_hold;
            }
            else
            {
                hold = gamepad_hold;
            }
            if (procon_lstick.x <= 0.05 &&
                procon_lstick.x >= -0.05 &&
                procon_lstick.y <= 0.05 &&
                procon_lstick.y >= -0.05)
            {
                lstick = gamepad_lstick;
            }
            else
            {
                lstick = procon_lstick;
            }

            if (procon_rstick.x <= 0.05 &&
                procon_rstick.x >= -0.05 &&
                procon_rstick.y <= 0.05 &&
                procon_rstick.y >= -0.05)
            {
                rstick = gamepad_rstick;
            }
            else
            {
                rstick = procon_rstick;
            }
            payload = formatInput(hold, lstick, rstick);
            write(client_fd, payload.c_str(), strlen(payload.c_str()));
        }
        NotificationModule_AddInfoNotification("Input Viewer Disconnected!");
        if (client_fd > 0)
        {
            close(client_fd);
        }
        close(server_fd);
        delete[] buffer;
        threadRunning = false;
}
/**
 * Callback that will be called if the config has been changed
 */
void boolItemChanged(ConfigItemBoolean *item, bool newValue)
{
    if (std::string_view(ENABLE_NIS) == item->identifier)
    {
        enableNIS = newValue;
        // If the value has changed, we store it in the storage.
        WUPSStorageAPI::Store(item->identifier, newValue);

    }
}

WUPSConfigAPICallbackStatus ConfigMenuOpenedCallback(WUPSConfigCategoryHandle rootHandle)
{
    // To use the C++ API, we create new WUPSConfigCategory from the root handle!
    WUPSConfigCategory root = WUPSConfigCategory(rootHandle);

    // The functions of the Config API come in two variants: One that throws an exception, and another one which doesn't
    // To use the Config API without exception see the example below this try/catch block.
    try
    {

        // We can also add items directly to root!
        root.add(WUPSConfigItemBoolean::Create(ENABLE_NIS, "Enable NIS",
                                               true, enableNIS,
                                               &boolItemChanged));

        // You can also add an item which just displays any text.
        uint32_t hostIpAddress = 0;
        nn::ac::GetAssignedAddress(&hostIpAddress);
        char ipSettings[50];
        if (hostIpAddress != 0)
        {
            snprintf(ipSettings,
                     50,
                     "Enter this address in OJD: %u.%u.%u.%u",
                     (hostIpAddress >> 24) & 0xFF,
                     (hostIpAddress >> 16) & 0xFF,
                     (hostIpAddress >> 8) & 0xFF,
                     (hostIpAddress >> 0) & 0xFF);
        }
        else
        {
            snprintf(
                ipSettings, sizeof(ipSettings), "The console is not connected to a network.");
        }
        root.add(WUPSConfigItemStub::Create(ipSettings));
    }
    catch (std::exception &e)
    {
        DEBUG_FUNCTION_LINE_ERR("Creating config menu failed: %s", e.what());
        return WUPSCONFIG_API_CALLBACK_RESULT_ERROR;
    }

    return WUPSCONFIG_API_CALLBACK_RESULT_SUCCESS;
}

void ConfigMenuClosedCallback()
{
    WUPSStorageAPI::SaveStorage();
}

INITIALIZE_PLUGIN()
{
    initLogging();
    NotificationModule_InitLibrary();
    procon_hold = std::bitset<19>(0).to_string();

    procon_lstick.x = 0;
    procon_lstick.y = 0;

    procon_rstick.x = 0;
    procon_rstick.y = 0;

    WUPSConfigAPIOptionsV1 configOptions = {.name = "Network Input Server"};
    if (WUPSConfigAPI_Init(configOptions, ConfigMenuOpenedCallback, ConfigMenuClosedCallback) != WUPSCONFIG_API_RESULT_SUCCESS)
    {
        DEBUG_FUNCTION_LINE_ERR("Failed to init config api");
    }

    WUPSStorageError storageRes;
    if ((storageRes = WUPSStorageAPI::GetOrStoreDefault(ENABLE_NIS, enableNIS, ENABLE_NIS_DEFAULT_VALUE)) != WUPS_STORAGE_ERROR_SUCCESS)
    {
        DEBUG_FUNCTION_LINE_ERR("GetOrStoreDefault failed: %s (%d)", WUPSStorageAPI_GetStatusStr(storageRes), storageRes);
    }
    if ((storageRes = WUPSStorageAPI::SaveStorage()) != WUPS_STORAGE_ERROR_SUCCESS)
    {
        DEBUG_FUNCTION_LINE_ERR("GetOrStoreDefault failed: %s (%d)", WUPSStorageAPI_GetStatusStr(storageRes), storageRes);
    }
    deinitLogging();
}

ON_APPLICATION_START() {
    initLogging();
    DEBUG_FUNCTION_LINE("ON_APPLICATION_START of example_plugin!");
}

DECL_FUNCTION(int32_t, VPADRead, VPADChan chan, VPADStatus *buffer, uint32_t buffer_size, VPADReadError *error)
{
    VPADReadError real_error;
    int32_t result = real_VPADRead(chan, buffer, buffer_size, &real_error);
    
    if (result > 0 && real_error == VPAD_READ_SUCCESS)
    {
        uint32_t end = 1;
        // Fix games like TP HD
        if (VPADGetButtonProcMode(chan) == 1)
        {
            end = result;
        }

        for (uint32_t i = 0; i < end; i++)
        {
            if (enableNIS){
                if (!threadRunning){
                    threadRunning=true;
                    std::thread server(socket_thread);
                    server.detach();
                }
                int hold = buffer[i].hold;
                gamepad_hold = std::bitset<19>(hold).to_string();
                gamepad_lstick = buffer->leftStick;
                gamepad_rstick = buffer->rightStick;
            }
            
        }
    }
    if (error)
    {
        *error = real_error;
    }
    return result;
}
DECL_FUNCTION(void, WPADRead, WPADChan chan, WPADStatusProController *data)
{
    real_WPADRead(chan, data);
    procon_hold = std::bitset<19>(0).to_string();

    procon_lstick.x = 0;
    procon_lstick.y = 0;

    procon_rstick.x = 0;
    procon_rstick.y = 0;
    

    if (data[0].err == 0 && enableNIS)
    {
        if (data[0].extensionType == WPAD_EXT_PRO_CONTROLLER)
        {
            procon_hold = std::bitset<19>(remapProButtons(data[0].buttons)).to_string();

            procon_lstick.x = data[0].leftStick.x / 1250.0;
            procon_lstick.y = data[0].leftStick.y / 1200.0;

            procon_rstick.x = data[0].rightStick.x / 1250.0;
            procon_rstick.y = data[0].rightStick.y / 1200.0;
        }
    }
}

WUPS_MUST_REPLACE(VPADRead, WUPS_LOADER_LIBRARY_VPAD, VPADRead);
WUPS_MUST_REPLACE(WPADRead, WUPS_LOADER_LIBRARY_PADSCORE, WPADRead);
