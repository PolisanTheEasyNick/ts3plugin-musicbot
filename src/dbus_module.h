#include <stdio.h>
#include <stdlib.h>
#include <dbus/dbus.h>
#include <string.h>

#define VLC_BUS_NAME "org.mpris.MediaPlayer2.vlc"
#define VLC_OBJECT_PATH "/org/mpris/MediaPlayer2"
#define VLC_TRACKLIST_INTERFACE "org.mpris.MediaPlayer2.TrackList"

char **track_list = NULL;
size_t track_count = 0;

typedef enum {
    ClubHits = 0,
    Breaks,
    SlapHouse,
    House,
    DeepOrganicHouse,
    Bassline,
    FutureGarage,
    BassAndJackingHouse,
    FutureBass,
    ChillAndTropicalHouse,
    ElectroSwing,
    ClubDubstep,
    VocalLounge,
    VocalChillout,
    LiquidDubstep,
    LiquidDnB,
    LatinHouse,
    Jungle,
    JazzHouse,
    Dubstep,
    Drumstep,
    Chillout,
    AtmosphericBreaks,
    Chillstep,
    DrumAndBass,
    DJMixes,
    Lounge,
    Ambient,
    FunkyHouse,
    SpaceDreams,
    ChilloutDreams,
    DiscoHouse
} RADIO_STATION;

void handle_dbus_error(DBusError *error) {
    if (dbus_error_is_set(error)) {
        fprintf(stderr, "DBus Error: %s\n", error->message);
        dbus_error_free(error);
        exit(EXIT_FAILURE);
    }
}

void get_track_list(DBusConnection *connection) {
    DBusMessage *message = NULL, *reply = NULL;
    DBusError error;
    dbus_error_init(&error);

    message = dbus_message_new_method_call(
        VLC_BUS_NAME,
        VLC_OBJECT_PATH,
        "org.freedesktop.DBus.Properties",
        "Get"
        );
    if (!message) {
        fprintf(stderr, "Failed to create DBus message\n");
        exit(EXIT_FAILURE);
    }

    const char *interface_name = VLC_TRACKLIST_INTERFACE;
    const char *property_name = "Tracks";
    dbus_message_append_args(
        message,
        DBUS_TYPE_STRING, &interface_name,
        DBUS_TYPE_STRING, &property_name,
        DBUS_TYPE_INVALID
        );

    reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);
    dbus_message_unref(message);
    handle_dbus_error(&error);

    DBusMessageIter args, array;
    if (dbus_message_iter_init(reply, &args)) {
        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
            dbus_message_iter_recurse(&args, &array);

            if (dbus_message_iter_get_arg_type(&array) == DBUS_TYPE_ARRAY) {
                dbus_message_iter_recurse(&array, &args);

                while (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_OBJECT_PATH) {
                    const char *path;
                    dbus_message_iter_get_basic(&args, &path);
                    track_list = (char**)realloc(track_list, sizeof(char *) * (track_count + 1));
                    track_list[track_count++] = strdup(path);
                    dbus_message_iter_next(&args);
                }
            }
        }
    }

    dbus_message_unref(reply);
}

void change_station(DBusConnection *connection, size_t station_index) {
    if (station_index >= track_count) {
        fprintf(stderr, "Invalid station index: %zu\n", station_index);
        return;
    }

    const char *track_path = track_list[station_index];
    DBusMessage *message = NULL;
    DBusError error;
    dbus_error_init(&error);

    message = dbus_message_new_method_call(
        VLC_BUS_NAME,
        VLC_OBJECT_PATH,
        VLC_TRACKLIST_INTERFACE,
        "GoTo"
        );
    if (!message) {
        fprintf(stderr, "Failed to create DBus message\n");
        exit(EXIT_FAILURE);
    }

    dbus_message_append_args(
        message,
        DBUS_TYPE_OBJECT_PATH, &track_path,
        DBUS_TYPE_INVALID
        );

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);
    dbus_message_unref(message);
    handle_dbus_error(&error);

    printf("Changed to station: %zu (%s)\n", station_index, track_path);

    dbus_message_unref(reply);
}

char *GetSongName(DBusConnection *connection) {
    DBusMessage *message = NULL, *reply = NULL;
    DBusError error;
    dbus_error_init(&error);

    message = dbus_message_new_method_call(
        "org.mpris.MediaPlayer2.vlc",  // VLC Bus Name
        "/org/mpris/MediaPlayer2",     // VLC Object Path
        "org.freedesktop.DBus.Properties", // Interface
        "Get"                          // Method
        );
    if (!message) {
        fprintf(stderr, "Failed to create DBus message\n");
        return NULL;
    }

    const char *interface_name = "org.mpris.MediaPlayer2.Player";
    const char *property_name = "Metadata";
    dbus_message_append_args(
        message,
        DBUS_TYPE_STRING, &interface_name,
        DBUS_TYPE_STRING, &property_name,
        DBUS_TYPE_INVALID
        );

    reply = dbus_connection_send_with_reply_and_block(connection, message, -1, &error);
    dbus_message_unref(message);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "DBus Error: %s\n", error.message);
        dbus_error_free(&error);
        return NULL;
    }

    DBusMessageIter args;
    char *song_name = NULL;

    if (dbus_message_iter_init(reply, &args) &&
        dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_VARIANT) {
        DBusMessageIter variant_iter, dict_iter, entry_iter;
        dbus_message_iter_recurse(&args, &variant_iter);
        if (dbus_message_iter_get_arg_type(&variant_iter) == DBUS_TYPE_ARRAY) {
            dbus_message_iter_recurse(&variant_iter, &dict_iter);

            while (dbus_message_iter_get_arg_type(&dict_iter) == DBUS_TYPE_DICT_ENTRY) {
                dbus_message_iter_recurse(&dict_iter, &entry_iter);

                if (dbus_message_iter_get_arg_type(&entry_iter) == DBUS_TYPE_STRING) {
                    const char *key;
                    dbus_message_iter_get_basic(&entry_iter, &key);

                    dbus_message_iter_next(&entry_iter);
                    if (strcmp(key, "vlc:nowplaying") == 0) {
                        if (dbus_message_iter_get_arg_type(&entry_iter) == DBUS_TYPE_VARIANT) {
                            DBusMessageIter value_iter;
                            dbus_message_iter_recurse(&entry_iter, &value_iter);

                            if (dbus_message_iter_get_arg_type(&value_iter) == DBUS_TYPE_STRING) {
                                dbus_message_iter_get_basic(&value_iter, &song_name);
                                break;
                            }
                        }
                    }
                }
                dbus_message_iter_next(&dict_iter);
            }
        }
    }


    dbus_message_unref(reply);

    if (song_name) {
        return strdup(song_name);
    } else {
        fprintf(stderr, "Failed to retrieve song name\n");
        return NULL;
    }
}