/*
 * TeamSpeak 3 demo plugin
 *
 * Copyright (c) TeamSpeak Systems GmbH
 */

#if defined(WIN32) || defined(__WIN32__) || defined(_WIN32)
#pragma warning(disable : 4100) /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "teamspeak/public_definitions.h"
#include "teamspeak/public_errors.h"
#include "teamspeak/public_errors_rare.h"
#include "teamspeak/public_rare_definitions.h"
#include "ts3_functions.h"

#include "plugin.h"

///// MY SECTION //////////
#include "dbus_module.h"
#define DEFAULT_CHANNEL_ID 12304
uint64_t currentChannelID = 0;
anyID myClientID;
uint64_t currentConnHandlerID = 0;
DBusConnection *connection = NULL;
DBusError dbus_error;


//END OF MY SECTION
static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src)                                                                                                                                                                                                                           \
    {                                                                                                                                                                                                                                                          \
        strncpy(dest, src, destSize - 1);                                                                                                                                                                                                                      \
        (dest)[destSize - 1] = '\0';                                                                                                                                                                                                                           \
    }
#endif

#define PLUGIN_API_VERSION 26

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128

static char* pluginID = NULL;

#ifdef _WIN32
/* Helper function to convert wchar_T to Utf-8 encoded strings on Windows */
static int wcharToUtf8(const wchar_t* str, char** result)
{
    int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
    *result    = (char*)malloc(outlen);
    if (WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0) {
        *result = NULL;
        return -1;
    }
    return 0;
}
#endif

/*********************************** Required functions ************************************/
/*
 * If any of these required functions is not implemented, TS3 will refuse to load the plugin
 */

/* Unique name identifying this plugin */
const char* ts3plugin_name()
{
#ifdef _WIN32
    /* TeamSpeak expects UTF-8 encoded characters. Following demonstrates a possibility how to convert UTF-16 wchar_t into UTF-8. */
    static char* result = NULL; /* Static variable so it's allocated only once */
    if (!result) {
        const wchar_t* name = L"Music Bot Plugin";
        if (wcharToUtf8(name, &result) == -1) { /* Convert name into UTF-8 encoded result */
            result = "Music Bot Plugin";             /* Conversion failed, fallback here */
        }
    }
    return result;
#else
    return "Music Bot Plugin";
#endif
}

/* Plugin version */
const char* ts3plugin_version()
{
    return "1.2";
}

/* Plugin API version. Must be the same as the clients API major version, else the plugin fails to load. */
int ts3plugin_apiVersion()
{
    return PLUGIN_API_VERSION;
}

/* Plugin author */
const char* ts3plugin_author()
{
    /* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "Oleh Polisan";
}

/* Plugin description */
const char* ts3plugin_description()
{
    /* If you want to use wchar_t, see ts3plugin_name() on how to use */
    return "MusicBot controller";
}

/* Set TeamSpeak 3 callback functions */
void ts3plugin_setFunctionPointers(const struct TS3Functions funcs)
{
    ts3Functions = funcs;
}

int ts3plugin_init()
{
    printf("PLUGIN: init\n");
    unsigned int error;
    int connectionStatus;
    
    currentConnHandlerID = ts3Functions.getCurrentServerConnectionHandlerID();
    if(currentConnHandlerID != 0) {
        if ((error = ts3Functions.getConnectionStatus(currentConnHandlerID, &connectionStatus)) != ERROR_ok) {
            ts3Functions.logMessage("Error checking connection status", LogLevel_ERROR, "Plugin", 0);
            printf("Error code is: %d\n", error);
            return 1;
        }

        if (connectionStatus != STATUS_CONNECTION_ESTABLISHED) {
            ts3Functions.logMessage("No active connection found", LogLevel_WARNING, "Plugin", 0);
            printf("No active connection, skipping initialization\n");
            return 0;
        }

        

        if ((error = ts3Functions.getClientID(currentConnHandlerID, &myClientID)) != ERROR_ok) {
            ts3Functions.logMessage("Error retrieving client ID", LogLevel_ERROR, "Plugin", 0);
            printf("Error code is: %d\n", error);
            return 1;
        }

        if ((error =  ts3Functions.getChannelOfClient(currentConnHandlerID, myClientID, &currentChannelID)) != ERROR_ok) {
            ts3Functions.logMessage("Error retrieving current channel ID", LogLevel_ERROR, "Plugin", 0);
            printf("Error code is: %d\n", error);
            return 1;
        }
    } else {
        printf("Bot is not connected to any server.");
    }
    printf("Initializing DBus...\n");
    dbus_error_init(&dbus_error);
    connection = dbus_bus_get(DBUS_BUS_SESSION, &dbus_error);
    handle_dbus_error(&dbus_error);

    get_track_list(connection);

    printf("Initialized with values:\nDefault channel ID: %d\nCurrent channel ID: %ld\nClient ID: %d\nConnection ID: %ld\n", DEFAULT_CHANNEL_ID, currentChannelID, myClientID, currentConnHandlerID);
    return 0;
}

/* Custom code called right before the plugin is unloaded */
void ts3plugin_shutdown()
{
    printf("PLUGIN: shutdown\n");
    free(track_list);
    if (pluginID) {
        free(pluginID);
        pluginID = NULL;
    }
}

/****************************** Optional functions ********************************/
/*
 * Following functions are optional, if not needed you don't need to implement them.
 */
int ts3plugin_offersConfigure()
{
    return PLUGIN_OFFERS_NO_CONFIGURE;
}


/* Client changed current server connection handler */
void ts3plugin_currentServerConnectionChanged(uint64 serverConnectionHandlerID)
{
    printf("PLUGIN: currentServerConnectionChanged %llu (%llu)\n", (long long unsigned int)serverConnectionHandlerID, (long long unsigned int)ts3Functions.getCurrentServerConnectionHandlerID());
}


/* Required to release the memory for parameter "data" allocated in ts3plugin_infoData and ts3plugin_initMenus */
void ts3plugin_freeMemory(void* data)
{
    free(data);
}


int ts3plugin_requestAutoload()
{
    return 1; /* 1 = request autoloaded, 0 = do not request autoload */
}


/************************** TeamSpeak callbacks ***************************/
/*
 * Following functions are optional, feel free to remove unused callbacks.
 * See the clientlib documentation for details on each function.
 */

/* Clientlib */

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber)
{
    /* Some example code following to show how to use the information query functions. */

    if (newStatus == STATUS_CONNECTION_ESTABLISHED) { /* connection established and we have client and channels available */
        currentConnHandlerID = serverConnectionHandlerID;

        if (ts3Functions.getClientID(serverConnectionHandlerID, &myClientID) != ERROR_ok) {
            ts3Functions.logMessage("Error querying client ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }

        if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, myClientID, &currentChannelID) != ERROR_ok) {
                ts3Functions.logMessage("Error querying channel ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        }
    }
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage) {
    printf("on client move event\nclient id: %d\nold channel id: %ld\nnew channel id: %ld\nmy client id: %d\n", clientID, oldChannelID, newChannelID, myClientID);
    int error;
    if(clientID == myClientID) {
        printf("Hehe i got to another channel manually!\n");
        printf("Setting old channel codec to voice..\n");
        if (oldChannelID != 0) {
            if ((error = ts3Functions.setChannelVariableAsInt(serverConnectionHandlerID, oldChannelID, CHANNEL_CODEC, CODEC_OPUS_VOICE)) != ERROR_ok) {
                ts3Functions.logMessage("Failed to set old channel codec to voice", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                printf("At client move event error num: %d\n", error);
            } else {
                ts3Functions.flushChannelUpdates(serverConnectionHandlerID, oldChannelID, "");
                printf("Old channel codec set to voice.\n");
            }
        }

        printf("Setting new channel codec to music..\n");
        if ((error = ts3Functions.setChannelVariableAsInt(serverConnectionHandlerID, newChannelID, CHANNEL_CODEC, CODEC_OPUS_MUSIC)) != ERROR_ok) {
            ts3Functions.logMessage("Failed to set new channel codec to music", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            printf("At client move event error num: %d\n", error);
        } else {
            ts3Functions.flushChannelUpdates(serverConnectionHandlerID, newChannelID, "");
            printf("New channel codec set to music.\n");
        }


        currentChannelID = newChannelID;
    } else {
        printf("Somebody else moved... perhaps i'm alone in current channel??\n");
        anyID *clientsInChannel;
        if (ts3Functions.getChannelClientList(serverConnectionHandlerID, currentChannelID, &clientsInChannel) != ERROR_ok) {
            ts3Functions.logMessage("Error getting client list for current channel", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            return;
        }
        size_t clientCount = 0;
        for (int i = 0; clientsInChannel[i]; i++) {
            clientCount++;
        }
        
        ts3Functions.freeMemory(clientsInChannel); 
        printf("Client count in current channel: %ld\n", clientCount);
        if (clientCount == 1 && currentChannelID != DEFAULT_CHANNEL_ID) {  
            printf("I'm alone in the channel. Moving to default channel (ID: %d)...\n", DEFAULT_CHANNEL_ID);
            if (ts3Functions.requestClientMove(serverConnectionHandlerID, myClientID, DEFAULT_CHANNEL_ID, "", "") != ERROR_ok) {
                ts3Functions.logMessage("Failed to move to default channel", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            }
            printf("Created move request!\n");
        } else {
            printf("Not alone or already in default, no need to move.\n");
        }
    }
}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char *moverName, const char *moverUniqueIdentifier, const char *moveMessage) {
    if(clientID == myClientID) {
        printf("Hey! I'm moved!\n");
        currentChannelID = newChannelID;
        int error;
        printf("Setting old channel codec to voice..\n");
        if (oldChannelID != 0) {
            if ((error = ts3Functions.setChannelVariableAsInt(serverConnectionHandlerID, oldChannelID, CHANNEL_CODEC, CODEC_OPUS_VOICE)) != ERROR_ok) {
                ts3Functions.logMessage("Failed to set old channel codec to voice", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
                printf("At client move event error num: %d\n", error);
            } else {
                ts3Functions.flushChannelUpdates(serverConnectionHandlerID, oldChannelID, "");
                printf("Old channel codec set to voice.\n");
            }
        }

        printf("Setting new channel codec to music..\n");
        if ((error = ts3Functions.setChannelVariableAsInt(serverConnectionHandlerID, newChannelID, CHANNEL_CODEC, CODEC_OPUS_MUSIC)) != ERROR_ok) {
            ts3Functions.logMessage("Failed to set new channel codec to music", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            printf("At client move event error num: %d\n", error);
        } else {
            ts3Functions.flushChannelUpdates(serverConnectionHandlerID, newChannelID, "");
            printf("New channel codec set to music.\n");
        }
    }
}


int ts3plugin_onTextMessageEvent(uint64 serverConnectionHandlerID, anyID targetMode, anyID toID, anyID fromID, const char* fromName, const char* fromUniqueIdentifier, const char* message, int ffIgnored)
{
    printf("PLUGIN: onTextMessageEvent %llu %d %d %s %s %d\n", 
           (long long unsigned int)serverConnectionHandlerID, targetMode, fromID, fromName, message, ffIgnored);
    
    if(fromID == myClientID) return 1;

    if (ffIgnored) {
        return 1;
    }

    if (targetMode != TextMessageTarget_CLIENT) {
        return 1;
    }

    uint64 senderChannelID;
    if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, fromID, &senderChannelID) != ERROR_ok) {
        ts3Functions.logMessage("Error querying sender channel ID", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        return 1;
    }

    if (currentChannelID != senderChannelID) {
        char sorryMessage[256];
        snprintf(sorryMessage, sizeof(sorryMessage), 
                 "Sorry %s, I can only respond to clients in the same room.", fromName);
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, sorryMessage, fromID, NULL);
        return 0; 
    }

    // Command handling
    if (strcmp(message, "!list") == 0 || strcmp(message, "!help") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID,
            "Available commands:\n"
            "!list or !help - Display this help message\n"
            "!song - Current song name\n"
            "!00 - 00 Club Hits Station\n"
            "!breaks - Breaks Station\n"
            "!slap_house - Slap House Station\n"
            "!house - House Station\n"
            "!deep_organic_house - Deep Organic House Station\n"
            "!bassline - Bassline Station\n"
            "!future_garage - Future Garage station\n"
            "!bnj - Bass & Jackin' House station\n"
            "!fb - Future Bass station\n"
            "!cnth - Chill & Tropical House station\n"
            "!ew - Electro Swing station\n"
            "!cb - Club Dubstep station\n"
            "!vl - Vocal Lounge station\n"
            "!vc - Vocal Chillout station\n"
            "!ld - Liquid Dubstep station\n"
            "!ldnb - Liquid DnB station\n"
            "!lh - Latin House station\n"
            "!jung - Jungle station\n"
            "!jh - Jazz House station\n"
            "!dub - Dubstep station\n"
            "!drum - DrumStep station\n"
            "!chill - Chillout station\n"
            "!ab - Atmospheric Breaks station\n"
            "!cs - Chillstep station\n"
            "!dnb - Drum and Bass station\n"
            "!mix - Dj Mixes station\n"
            "!lounge - Lounge station\n"
            "!ambient - Ambient station\n"
            "!funky - Funky House station\n"
            "!space - Space Dreams station\n"
            "!cd - Chillout Dreams station\n"
            "!disco - Disco House station", 
            fromID, NULL);
    } else if (strcmp(message, "!song") == 0) {
        printf("Get current song request, getting...");
        char *song_name = GetSongName(connection);
        if (song_name) {
            printf("Currently playing: %s\n", song_name);
            char message[256];
            snprintf(message, sizeof(message), "[b]Currently playing:[/b] [i]%s[/i]", song_name);
            ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, message, fromID, NULL);
            free(song_name);
        } else {
            printf("Failed to retrieve song name\n");
            ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Sorry, got unexcepted error while getting current song :c", fromID, NULL);
        }
    } else if (strcmp(message, "!00") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into 00s Club Hits station!", fromID, NULL);
        change_station(connection, ClubHits);
    } else if (strcmp(message, "!breaks") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Breaks station!", fromID, NULL);
        change_station(connection, Breaks);
    } else if (strcmp(message, "!slap_house") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Slap House station!", fromID, NULL);
        change_station(connection, SlapHouse);
    } else if (strcmp(message, "!house") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into House station!", fromID, NULL);
        change_station(connection, House);
    } else if (strcmp(message, "!deep_organic_house") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Deep Organic House station!", fromID, NULL);
        change_station(connection, DeepOrganicHouse);
    } else if (strcmp(message, "!bassline") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Bassline station!", fromID, NULL);
        change_station(connection, Bassline);
    } else if (strcmp(message, "!future_garage") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Future Garage station!", fromID, NULL);
        change_station(connection, FutureGarage);
    } else if (strcmp(message, "!bnj") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Bass & Jackin' House station!", fromID, NULL);
        change_station(connection, BassAndJackingHouse);
    } else if (strcmp(message, "!fb") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Future Bass station!", fromID, NULL);
        change_station(connection, FutureBass);
    } else if (strcmp(message, "!cnth") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Chill & Tropical House station!", fromID, NULL);
        change_station(connection, ChillAndTropicalHouse);
    } else if (strcmp(message, "!ew") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Electro Swing station!", fromID, NULL);
        change_station(connection, ElectroSwing);
    } else if (strcmp(message, "!cb") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Club Dubstep station!", fromID, NULL);
        change_station(connection, ClubDubstep);
    } else if (strcmp(message, "!vl") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Vocal Lounge station!", fromID, NULL);
        change_station(connection, VocalLounge);
    } else if (strcmp(message, "!vc") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Vocal Chillout station!", fromID, NULL);
        change_station(connection, VocalChillout);
    } else if (strcmp(message, "!ld") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Liquid Dubstep station!", fromID, NULL);
        change_station(connection, LiquidDubstep);
    } else if (strcmp(message, "!ldnb") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Liquid DnB station!", fromID, NULL);
        change_station(connection, LiquidDnB);
    } else if (strcmp(message, "!lh") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Latin House station!", fromID, NULL);
        change_station(connection, LatinHouse);
    } else if (strcmp(message, "!jung") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Jungle station!", fromID, NULL);
        change_station(connection, Jungle);
    } else if (strcmp(message, "!jh") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Jazz House station!", fromID, NULL);
        change_station(connection, JazzHouse);
    } else if (strcmp(message, "!dub") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Dubstep station!", fromID, NULL);
        change_station(connection, Dubstep);
    } else if (strcmp(message, "!drum") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Drumstep station!", fromID, NULL);
        change_station(connection, Drumstep);
    } else if (strcmp(message, "!chill") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Chillout station!", fromID, NULL);
        change_station(connection, Chillout);
    } else if (strcmp(message, "!ab") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Atmospheric Breaks station!", fromID, NULL);
        change_station(connection, AtmosphericBreaks);
    } else if (strcmp(message, "!cs") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Chillstep station!", fromID, NULL);
        change_station(connection, Chillstep);
    } else if (strcmp(message, "!dnb") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Drum and Bass station!", fromID, NULL);
        change_station(connection, DrumAndBass);
    } else if (strcmp(message, "!mix") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into DJ Mixes station!", fromID, NULL);
        change_station(connection, DJMixes);
    } else if (strcmp(message, "!lounge") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Lounge station!", fromID, NULL);
        change_station(connection, Lounge);
    } else if (strcmp(message, "!ambient") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Ambient station!", fromID, NULL);
        change_station(connection, Ambient);
    } else if (strcmp(message, "!funky") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Funky House station!", fromID, NULL);
        change_station(connection, FunkyHouse);
    } else if (strcmp(message, "!space") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Space Dreams station!", fromID, NULL);
        change_station(connection, SpaceDreams);
    } else if (strcmp(message, "!cd") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Chillout Dreams station!", fromID, NULL);
        change_station(connection, ChilloutDreams);
    } else if (strcmp(message, "!disco") == 0) {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, "Tuning into Disco House station!", fromID, NULL);
        change_station(connection, DiscoHouse);
    }
    else {
        ts3Functions.requestSendPrivateTextMsg(serverConnectionHandlerID, 
            "Unknown command. Type !list or !help to see available commands.", fromID, NULL);
    }

    return 0;
}

void ts3plugin_onClientKickFromChannelEvent (uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID kickerID, const char *kickerName, const char *kickerUniqueIdentifier, const char *kickMessage) {
    printf("Client kicked from channel!!!\n");
    printf("Setting old channel codec to voice..\n");
    int error;
    if (oldChannelID != 0) {
        if ((error = ts3Functions.setChannelVariableAsInt(serverConnectionHandlerID, oldChannelID, CHANNEL_CODEC, CODEC_OPUS_VOICE)) != ERROR_ok) {
            ts3Functions.logMessage("Failed to set old channel codec to voice", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
            printf("At client move event error num: %d\n", error);
        } else {
            ts3Functions.flushChannelUpdates(serverConnectionHandlerID, oldChannelID, "");
            printf("Old channel codec set to voice.\n");
        }
    }

    printf("Moving to default channel (ID: %d)...\n", DEFAULT_CHANNEL_ID);
    if (ts3Functions.requestClientMove(serverConnectionHandlerID, myClientID, DEFAULT_CHANNEL_ID, "", "") != ERROR_ok) {
        ts3Functions.logMessage("Failed to move to default channel", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
    }
    printf("Created move request!\n");

    if ((error = ts3Functions.setChannelVariableAsInt(serverConnectionHandlerID, DEFAULT_CHANNEL_ID, CHANNEL_CODEC, CODEC_OPUS_MUSIC)) != ERROR_ok) {
        ts3Functions.logMessage("Failed to set old channel codec to voice", LogLevel_ERROR, "Plugin", serverConnectionHandlerID);
        printf("At client move event error num: %d\n", error);
    } else {
        ts3Functions.flushChannelUpdates(serverConnectionHandlerID, DEFAULT_CHANNEL_ID, "");
        printf("Old channel codec set to voice.\n");
    }
}