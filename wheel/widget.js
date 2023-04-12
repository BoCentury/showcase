//~ START

// @Globals
let the_wheel;
let field_data;
let spin_duration;

let current_sound;
let default_sound;
let vip_sound;

let queue = Array(16).fill({});
let queue_count = 0;
let queue_index = 0;

let wheel_spinning = false;

let is_editor = false;

let lplus_info = {};

let wheels = new Array(1 + 9);
wheels[0] = null; // Winwheel's zero index is also null, for consistency

let tts_three = make_tts("three");
let tts_two = make_tts("two");

//~ Helpers

function get_timedate_now() {
    return new Date().toLocaleString('en-GB', { dateStyle: "short", timeStyle: "short", timeZone: 'UTC' });
}

function play_sound(a) {
    a.pause();
    a.currentTime = 0;
    a.play();
}

function make_tts(text) {
    const tts = new Audio();
    tts.src = "https://api.streamelements.com/kappa/v2/speech?voice=Brian&text=" + encodeURIComponent(text);
    tts.volume = 0.9;
    return tts;
}

function speak(text) {
    make_tts(text).play();
}

function timer_countdown_speak(done_text, time) {
    let done_tts = make_tts(`one. ${done_text}`);
    setTimeout(() => {
                   play_sound(tts_three);
                   setTimeout(() => {
                                  play_sound(tts_two);
                                  setTimeout(() => {
                                                 play_sound(done_tts);
                                             }, 1000);
                              }, 1000);
               }, time - 3000);
}

//~ SE store

// @Globals
let globals = {
    sub_wheel_enabled: false,
    log: "",
    clear_log: false,
};
const store_keyname = "candiceWheelKeyname";

function callback_get_from_store(obj) {
    if(typeof obj.sub_wheel_enabled === "boolean") globals.sub_wheel_enabled = obj.sub_wheel_enabled;
    if(typeof obj.log === "string") globals.log = obj.log;
    if(typeof obj.clear_log === "boolean") globals.clear_log = obj.clear_log;
    setTimeout(() => {
                   console.log("Log:\n" + globals.log);
               }, 1000);
}

function get_from_store() {
    SE_API.store.get(store_keyname).then(callback_get_from_store);
}
get_from_store();

function save_globals() { SE_API.store.set(store_keyname, globals); }

function clear_log() {
    globals.clear_log = true;
    save_globals();
}

function print_log() { console.log(globals.log); }

//~ Twitch auth

// @Globals
let channel_id = /*INSERT CHANNEL ID HERE*/; // TODO(bo): Get this from SE in callback_on_widget_load or as an input field
let bot_id;
let client_id = "";
let oauth_token = "";
let twitch_auth_good = false;

function parse_auth(text) {
    let result = {
        login: "",
        user_id: "",
        client_id: "",
        oauth_token: "",
    };
    if(text) {
        let parts = text.split(";");
        for(let i = 0; i < parts.length; i++) {
            let part = parts[i];
            if(part.startsWith("username=")) {
                result.login = part.slice(9);
            } else if(part.startsWith("user_id=")) {
                result.user_id = part.slice(8);
            } else if(part.startsWith("client_id=")) {
                result.client_id = part.slice(10);
            } else if(part.startsWith("oauth_token=")) {
                result.oauth_token = part.slice(12);
            }
        }
    }
    return result;
}

//~ Wheel info stuff

function make_segments(text) {
    let segment_infos = JSON.parse(text);
    let segments = Array(null);
    for(let i = 0; i < segment_infos.length; i++) {
        segments[i + 1] = new Segment(segment_infos[i]);
    }
    
    // Fill in segement sizes
    let total_degrees_reserved = 0;
    let number_of_segments_with_reserved_size = 0;
    
    for(let i = 1; i < segments.length; i++) {
        if(segments[i].size !== null) {
            total_degrees_reserved += segments[i].size;
            number_of_segments_with_reserved_size++;
        }
    }
    
    let unreserved_degrees = 360 - total_degrees_reserved;
    let degrees_each = 0;
    if(unreserved_degrees > 0) degrees_each = unreserved_degrees / (segments.length - 1 - number_of_segments_with_reserved_size);
    
    let at = 0;
    for(let i = 1; i < segments.length; i++) {
        segments[i].startAngle = at;
        at += segments[i].size ? segments[i].size : degrees_each;
        segments[i].endAngle = at;
    }
    return segments;
}

function make_wheel_info(index) {
    let result = {
        img: new Image(),
        segments: make_segments(field_data[`wheel${index}SegmentsText`]),
        reward_id: field_data[`wheel${index}RewardId`],
    };
    result.img.src = field_data[`wheel${index}Image`];
    return result;
}

//~ Wheel stuff

function get_index_of_segment_its_gonna_land_on() {
    let raw_angle = the_wheel.animation._stopAngle % 360;
    if(raw_angle < 0) raw_angle = (360 + raw_angle); // + cause it's negative
    
    // Now we have the angle of the wheel, but we need to take in to account where the pointer is because
    // will not always be at the 12 o'clock 0 degrees location.
    let relative_angle = Math.floor(the_wheel.pointerAngle - raw_angle);
    if(relative_angle < 0) relative_angle = 360 + relative_angle; // + cause it's negative
    
    let landed_index = 0;
    // Now we can work out the prize won by seeing what prize segment startAngle and endAngle the relative_angle is between.
    for(let i = 1; i < (the_wheel.segments.length); i++) {
        if((relative_angle >= the_wheel.segments[i].startAngle) &&
           (relative_angle <= the_wheel.segments[i].endAngle)) {
            landed_index = i;
            break;
        }
    }
    
    return landed_index;
}

function add_wheel_to_queue_and_maybe_start_dequeue(wheel_index, stop_angle) {
    if(wheel_index < 1 || wheel_index > wheels.length - 1) return;
    if(queue_count >= queue.length) {
        console.log("Wheel queue overflow");
        return;
    }
    let queue_free_slot = (queue_index + queue_count) % queue.length;
    queue[queue_free_slot] = {};
    queue[queue_free_slot].wheel_index = wheel_index;
    if(typeof stop_angle !== "undefined") {
        queue[queue_free_slot].stop_angle = stop_angle;
    }
    queue_count += 1;
    if(!wheel_spinning) {
        dequeue_and_spin_wheel();
    }
}

let vip_wheel_user_id = ""; // @Globals

function dequeue_and_spin_wheel() {
    wheel_spinning = true;
    let wheel_index = queue[queue_index].wheel_index;
    let stop_angle = 0;
    let stop_angle_was_specified = typeof queue[queue_index].stop_angle !== "undefined";
    if(stop_angle_was_specified) {
        stop_angle = queue[queue_index].stop_angle;
    } else {
        stop_angle = Math.floor(Math.random() * 360);
    }
    queue_count -= 1;
    queue_index += 1;
    if(queue_index >= queue.length) queue_index = 0;
    
    the_wheel.wheelImage = wheels[wheel_index].img;
    the_wheel.numSegments = wheels[wheel_index].segments.length - 1;
    the_wheel.segments = wheels[wheel_index].segments;
    the_wheel.draw();
    let is_vip_wheel = wheel_index === 1;
    if(is_vip_wheel) {
        current_sound = vip_sound;
    } else {
        current_sound = default_sound;
    }
    
    document.getElementById("container").style.visibility = "visible";
    the_wheel.rotationAngle = 0;
    the_wheel.stopAnimation(false);
    the_wheel.animation.stopAngle = stop_angle;
    
    play_sound(current_sound);
    the_wheel.startAnimation();
    
    let winning_index = get_index_of_segment_its_gonna_land_on();
    let winning_segment = the_wheel.segments[winning_index];
    
    last_spin_wheel_index = winning_index;
    last_spin_stop_angle = stop_angle;
    
    //console.log("It's gonna land on " + winning_segment.text);
    let wheel_tts = make_tts(winning_segment.text);
    if(wheel_index === 8 && !stop_angle_was_specified) { // 8 is liquid+ points wheel
        globals.log += get_timedate_now() + "\n- Wheel\n" +
            "Twitch: " + lplus_info.twitch_name + "\n" +
            "Liquid+: " + lplus_info.lplus_name + "\n" +
            "Points: " + winning_segment.text + "\n\n";
        save_globals();
    }
    
    let user_id = vip_wheel_user_id;
    setTimeout(() => {
                   play_sound(wheel_tts);
                   
                   if(winning_segment.text === "stand still") {
                       timer_countdown_speak("you can move now", 30000);
                   } else if(winning_segment.text === "no shield heal") {
                       timer_countdown_speak("you can shield heal now", 45000);
                   } else if(winning_segment.text === "unbind shift") {
                       timer_countdown_speak("you can rebind shift now", 60000);
                   } else if(winning_segment.text === "L") {
                       setTimeout(() => {
                                      if(user_id) {
                                          timeout_twitch_user_id(user_id, 600, "Automated by Bo's bot");
                                      }
                                  }, 5000);
                   }
                   setTimeout(() => {
                                  if(queue_count > 0) {
                                      dequeue_and_spin_wheel();
                                  } else {
                                      wheel_spinning = false;
                                      document.getElementById("container").style.visibility = "hidden";
                                  }
                              }, 5000);
               }, spin_duration * 1000 - 200);
}

//~ Message handling

let last_spin_wheel_index = 0; // @Globals
let last_spin_stop_angle = 0; // @Globals

function handle_commands(text, nick) {
    if(nick === /*INSERT STREAMER NAME HERE*/ || nick === "bocentury") {
        let parts = text.replace(/\s+/g, " ").trim().split(" ", 2);
        if(parts[0] === "!redospin") {
            if(last_spin_wheel_index > 0) {
                speak("redo queued");
                add_wheel_to_queue_and_maybe_start_dequeue(last_spin_wheel_index, last_spin_stop_angle);
            } else {
                speak("no spin to redo");
            }
        } else if(parts[0] === "!spinvip") {
            let username = parts[1];
            if(username.startsWith("@")) username = username.slice(1);
            
            helix_get_user_id(username, id => {
                                  vip_wheel_user_id = id;
                                  add_wheel_to_queue_and_maybe_start_dequeue(1);
                              });
        }
    }
}

/*function _old_handle_commands(text, nick) {
	if(text.startsWith("!subwheelon")) {
    	globals.sub_wheel_enabled = true;
        save_globals();
	} else if(text.startsWith("!subwheeloff")) {
    	globals.sub_wheel_enabled = false;
        save_globals();
	} else if(nick === "bocentury") {
        if(text.startsWith("!spintest")) {
	        speak("test spin");
            let wheel_index = parseInt(text.slice(9));
            if(typeof wheel_index === "number") {
                add_wheel_to_queue_and_maybe_start_dequeue(wheel_index);
            }
	    } else if(text.startsWith("!spinmanual")) {
            let wheel_index = parseInt(text.slice(11));
            if(typeof wheel_index === "number") {
                let count = 1;
                let pos = text.indexOf(" ");
                if(pos !== -1) {
                    while(text.charCodeAt(pos) === 32) pos++; // 32 = ' '
                    let num = parseInt(text.slice(pos));
                    if(typeof num === "number" && num > 1) {
                        count = (num > queue.length) ? queue.length : num;
                    }
                }
                for(let i = 0; i < count; i++) {
                    add_wheel_to_queue_and_maybe_start_dequeue(wheel_index);
                }
            }
	    } else if(text.startsWith("!wheeltts ")) {
	        let tts = text.slice(10);
            if(tts) speak(tts);
	    } else if(text.startsWith("!wheelclearqueue")) {
            queue_count = 0;
	    } else if(text.startsWith("!wheeltimeouttest ")) {
            let username = text.slice(14);
            if(username) timeout_twitch_user_login(username, 5, "!wheeltimeouttest");
        }
	}
}*/



//~ StreamElements event stuff ('init' and 'tick')

// Array of events coming to widget that are not queued so they can come even queue is on hold
const skippable_events = ["bot:counter", "event:test", "event:skip"]; // @Globals

window.addEventListener("onEventReceived", callback_on_event_received);

function callback_on_event_received(obj) {
    if(skippable_events.indexOf(obj.detail.listener) !== -1) return;
    /*if(is_editor) {
        if(obj.detail.listener === "message" &&
           obj.detail.event.data.nick === "streamelements" &&
           obj.detail.event.data.text.indexOf(" just subscribed for ") !== -1) {
            console.log("bo__se_chat_message:\n" + JSON.stringify(obj.detail.event));
        } else if(obj.detail.listener === "subscriber-latest") {
            console.log("bo__subscriber_latest:\n" + JSON.stringify(obj.detail.event));
        } else if(obj.detail.listener === "event" &&
                  typeof obj.detail.event.type !== "undefined" && obj.detail.event.type === "subscriber") {
            let e = obj.detail.event;
            if(typeof e._id === "string" && typeof e.activityId === "string" && typeof e.data === "object" &&
               typeof e.data.username === "string" && typeof e.data.displayName === "string" &&
               typeof e.data.amount   === "number" && typeof e.data.tier        === "string") {
                console.log("bo__good_sub_event:\n" + JSON.stringify(obj.detail.event));
            } else {
                console.log("bo__bad_sub_event:\n" + JSON.stringify(obj.detail.event));
            }
        } else if(obj.detail.listener === "event" &&
                  typeof obj.detail.event.type !== "undefined" && obj.detail.event.type === "redemption") {
            console.log("bo__redemption_event:\n" + JSON.stringify(obj.detail.event));
        } else if(obj.detail.listener !== "message") {
            console.log("bo__other_event (" + obj.detail.listener + "):\n" + JSON.stringify(obj.detail.event));
        }
    }*/
    if(obj.detail.listener === "message") {
        // Spin on Channel Point Reward
        let data = obj.detail.event.data;
        let reward_id = data.tags["custom-reward-id"];
        if(reward_id !== undefined) {
            if(reward_id === wheels[8].reward_id) { // 8 is liquid+ points wheel
                // This only works cause there can only be one liquid+ points spin per stream.
                // If that changes in the future then we need to change it to add the info the queue
                if(data.displayName.toLowerCase() !== data.nick) {
                    lplus_info.twitch_name = data.displayName + " (" + data.nick + ")";
                } else {
                    lplus_info.twitch_name = data.displayName;
                }
                lplus_info.lplus_name = data.text;
            }
            for(let i = 1; i < wheels.length; i++) {
                if(reward_id === wheels[i].reward_id) {
                    if(i === 1) vip_wheel_user_id = data.userId;
                    add_wheel_to_queue_and_maybe_start_dequeue(i);
                    break;
                }
            }
        } else if(data.text.charCodeAt(0) === 33 && (data.tags.mod === "1" || data.nick === /*INSERT STREAMER NAME HERE*/)) { // 33 = '!'
            handle_commands(data.text, data.nick);
        }
    }
    /*else if(obj.detail.listener === "event" &&
                 typeof obj.detail.event.type !== "undefined" &&
                 obj.detail.event.type === "subscriber") {
           // Spin on sub
           let e = obj.detail.event;
           if(typeof e._id === "string" && typeof e.activityId === "string" && typeof e.data === "object" &&
              typeof e.data.username === "string" && typeof e.data.displayName === "string" &&
              typeof e.data.amount   === "number" && typeof e.data.tier        === "string") {
               //console.log("bo__good_sub_event:\n" + JSON.stringify(obj.detail.event));
               if(globals.sub_wheel_enabled) {
                   setTimeout(function() {
                                  add_wheel_to_queue_and_maybe_start_dequeue(9); // wheel 9 is currently the sub wheel
                              }, 6000);
               }
           } else {
               //console.log("bo__bad_sub_event:\n" + JSON.stringify(obj.detail.event));
           }
       }*/
    else {
        SE_API.resumeQueue();
    }
    
}

window.addEventListener("onWidgetLoad", callback_on_widget_load);

function callback_on_widget_load(obj) {
    //SE_API.store.get(store_keyname).then(obj => { logText = obj.value; });
    if(obj.detail.overlay.isEditorMode) {
        is_editor = true;
    }
    field_data = obj.detail.fieldData;
    spin_duration = field_data.duration;
    
    if(field_data.chatbot_state !== "disabled" &&
       (is_editor == (field_data.chatbot_state === "editor_only"))) {
        let info = parse_auth(field_data.chatbot_info);
        if(!info.user_id || !info.client_id || !info.oauth_token) {
            console.log("[???-chat] - bad chatbot_info");
        } else {
            bot_id = info.user_id;
            client_id = info.client_id;
            oauth_token = info.oauth_token;
            twitch_auth_good = true;
        }
    }
    
    if(!is_editor && globals.clear_log) {
        globals.clear_log = false;
        globals.log = "";
        save_globals();
    }
    
    let arrow = document.getElementById("arrow");
    // + 8 cause of 8 pixel margin being annoying
    arrow.style.left = (field_data.wheelSize / 2 - arrow.width / 2 + field_data.arrowX + 8) + "px";
    arrow.style.top = (field_data.wheelSize / 2 - arrow.height - field_data.arrowY + 8) + "px";
    
    for(let i = 1; i < wheels.length; i++) wheels[i] = make_wheel_info(i);
    
    the_wheel = new Winwheel({
                                 drawMode: "image",
                                 //imageOverlay: true,
                                 //lineWidth: 2,
                                 outerRadius: 300,
                                 //numSegments: wheels[1].segments.length,
                                 //segments: wheels[1].segments,
                                 animation: {
                                     type: "spinToStop",
                                     duration: spin_duration,
                                     spins: field_data.spins,
                                     //callbackSound: play_sound,
                                     //callbackFinished: spin_done_callback
                                 }
                             });
    default_sound = new Audio(field_data.defaultSound);
    vip_sound = new Audio(field_data.vipSound);
    current_sound = default_sound;
    
    if(field_data.showOnStart === "show") {
        setTimeout(() => {
                       document.getElementById("container").style.visibility = "visible";
                       let wheel_to_show = 9;
                       the_wheel.wheelImage = wheels[wheel_to_show].img;
                       the_wheel.numSegments = wheels[wheel_to_show].segments.length - 1;
                       the_wheel.segments = wheels[wheel_to_show].segments;
                       the_wheel.draw();
                   }, 200);
    }
}


//~ Twitch helix api stuff

function timeout_twitch_user_id(id, duration, reason) {
    if(!twitch_auth_good) return;
    if(!reason) reason = "";
    helix_timeout_user_id(id, duration, reason);
}

function timeout_twitch_user_login(login, duration, reason) {
    if(!twitch_auth_good) return;
    if(!reason) reason = "";
    helix_get_user_id(login, id => helix_timeout_user_id(id, duration, reason));
}

// helix_timeout_user_id(866437380, 300, "test 1").then((data) => console.log(data));
// Requires a user access token that includes the moderator:manage:banned_users scope.
function helix_timeout_user_id(user_id, duration, reason, optional_proc) {
    if(!twitch_auth_good) return;
    console.log("[???-helix] timeout: " + user_id + " " + duration + " " + reason);
    fetch("https://api.twitch.tv/helix/moderation/bans?broadcaster_id=" + channel_id + "&moderator_id=" + bot_id, {
              "headers": {
                  "authorization": "Bearer " + oauth_token,
                  "client-id": client_id,
                  "content-type": "application/json",
              },
              "body": "{\"data\": {\"user_id\":\"" + user_id + "\",\"duration\":" + duration + ",\"reason\":\"" + reason + "\"}}",
              "method": "POST",
          })
        .then((response) => response.json())
        .then((data) => {
                  if(typeof data.error === "string") {
                      console.log("[???-helix] timeout error:\n" + JSON.stringify(data));
                  } else if(typeof optional_proc !== "undefined") {
                      optional_proc(data.data[0]);
                  }
              });
}

//helix_get_user_id("bocenturybot", id => helix_timeout_user_id(id, 300, "test 3"));
// Requires an app access token or user access token.
function helix_get_user_id(user_login, proc) { // proc example: id => console.log(id)
    if(!twitch_auth_good) return;
    //console.log("[???-helix] user_id: "+user_login);
    fetch("https://api.twitch.tv/helix/users?login=" + user_login, {
              "headers": {
                  "authorization": "Bearer " + oauth_token,
                  "client-id": client_id,
              },
              "body": null,
              "method": "GET",
          })
        .then((response) => response.json())
        .then((data) => {
                  if(typeof data.error === "string") {
                      console.log("[???-helix] user_id error:\n" + JSON.stringify(data));
                  } else {
                      proc(data.data[0].id);
                  }
              })
        .catch((e) => console.error(e));
}

//~ END (the rest is commented out, unfinished, experimental, or untested stuff)

//- helix /mod /unmod. need to test the response

// helix_timeout_user_id(866437380, 300, "test 1").then((data) => console.log(data));
// Rate Limits: The broadcaster may add a maximum of 10 moderators within a 10-second window.
// Requires a user access token that includes the channel:manage:moderators scope.
function helix_add_moderator(user_id, optional_proc) {
    if(!twitch_auth_good) return;
    if(bot_id !== channel_id) return;
    console.log("[???-helix] /mod: " + user_id);
    
    fetch("https://api.twitch.tv/helix/moderation/moderators?broadcaster_id=" + channel_id + "&moderator_id=" + user_id, {
              "headers": {
                  "authorization": "Bearer " + oauth_token,
                  "client-id": client_id,
              },
              "body": null,
              "method": "POST",
          })
        .then((response) => response.json()) // TODO(bo): not sure what the respones would be exactly, test it
        .then((data) => {
                  if(typeof data.error === "string") {
                      console.log("[???-helix] /mod error:\n" + JSON.stringify(data));
                  } else if(typeof optional_proc !== "undefined") {
                      optional_proc(data.data[0]);
                  }
              });
}

// helix_timeout_user_id(866437380, 300, "test 1").then((data) => console.log(data));
// Rate Limits: The broadcaster may add a maximum of 10 moderators within a 10-second window.
// Requires a user access token that includes the channel:manage:moderators scope.
function helix_remove_moderator(user_id, optional_proc) {
    if(!twitch_auth_good) return;
    if(bot_id !== channel_id) return;
    console.log("[???-helix] /unmod: " + user_id);
    fetch("https://api.twitch.tv/helix/moderation/moderators?broadcaster_id=" + channel_id + "&moderator_id=" + user_id, {
              "headers": {
                  "authorization": "Bearer " + oauth_token,
                  "client-id": client_id,
              },
              "body": null,
              "method": "DELETE",
          })
        .then((response) => response.json()) // TODO(bo): not sure what the respones would be exactly, test it
        .then((data) => {
                  if(typeof data.error === "string") {
                      console.log("[???-helix] /unmod error:\n" + JSON.stringify(data));
                  } else if(typeof optional_proc !== "undefined") {
                      optional_proc(data.data[0]);
                  }
              });
}

//- Twitch chatbot that can't timeout anymore (thanks twitch)
// TODO(bo): Add this back in to announce some of the wheel results

let global = {
    channel_name: "",
    chat_username: "",
    chat_oauth: ""
};

let chat_server = null;
let modme_whitelist = [];

function initialize_and_start_chatbot(obj) {
    if(!field_data.chatbot_info) return;
    let info = parse_auth(field_data.chatbot_info);
    if(!info.login || !info.auth) {
        console.log("[???-chat] - bad chatbot_info");
        return;
    }
    try {
        modme_whitelist = JSON.parse("[" + field_data.modme_whitelist + "]");
    } catch (err) {
        modme_whitelist = [];
        console.log("JSON.parse error (modme_whitelist): " + err.message);
    }
    global.channel_name = obj.detail.channel.username;
    global.chat_username = info.login;
    global.chat_auth = info.auth;
    
    // Start the bot
    console.log("[???-chat] - starting...");
    setTimeout(connect_to_chat, 4000);
}

function connect_to_chat() {
    chat_server = new WebSocket("wss://irc-ws.chat.twitch.tv:443");
    chat_server.onopen = callback_on_chat_open;
    chat_server.onclose = callback_on_chat_closed;
    chat_server.onerror = callback_on_chat_error;
    chat_server.onmessage = callback_on_chat_message;
}

function send_twitch_message(message) {
    console.log("[???-chat] send_message: " + message);
    if(chat_server != null) {
        chat_server.send("PRIVMSG #" + global.channel_name + " :" + message);
    }
}

function callback_on_chat_open(obj) {
    console.log("[???-chat] - open");
    
    chat_server.send("CAP REQ :twitch.tv/tags twitch.tv/commands");
    chat_server.send("PASS oauth:" + global.chat_auth);
    chat_server.send("NICK " + global.chat_username);
}

let want_to_close = false;

function callback_on_chat_closed(obj) {
    console.log("[???-chat] - closed");
    
    chat_server = null;
    if(!want_to_close) {
        console.log("[???-chat] - attempting reconnect in 10s...");
        setTimeout(connect_to_chat, 10000);
    }
}

function callback_on_chat_error(obj) {
    console.log("[???-chat] - error");
    
    chat_server.close();
}

function callback_on_chat_message(obj) {
    const parts = event.data.trim().split("\r\n");
    for(let i = 0; i < parts.length; i++) {
        const message = parse_message(parts[i]);
        if(message) handle_message(message);
    }
}

function parse_tags(raw_tags_text) {
    let result = {
        is_mod: false,
        bit_amount: 0,
        display_name: "",
    };
    
    let raw_tags = raw_tags_text.split(";");
    for(let i = 0; i < raw_tags.length; i++) {
        let raw = raw_tags[i];
        let [name, value] = break_by_char(raw, "=");
        
        if(name === "badges") {
            if(value.indexOf("moderator") !== -1 || value.indexOf("broadcaster") !== -1) {
                result.is_mod = true;
            }
        } else if(name == "bits") {
            result.bit_amount = parseInt(value);
        } else if(name == "display-name") {
            result.display_name = value;
        }
    }
    
    return result;
}

function parse_message(data) {
    const message = {
        raw: data,
        raw_tags_text: "",
        prefix: null,
        command: null,
        params: []
    };
    
    let position = 0;
    let nextspace = 0;
    
    // tags
    if(data.charCodeAt(0) === 64) { // 64 = '@'
        nextspace = data.indexOf(" ");
        if(nextspace === -1) return null;
        
        message.raw_tags_text = data.slice(1, nextspace);
        position = nextspace + 1;
    }
    
    while(data.charCodeAt(position) === 32) position++; // 32 = ' '
    
    // prefix
    if(data.charCodeAt(position) === 58) { // 58 = ':'
        nextspace = data.indexOf(" ", position);
        if(nextspace === -1) return null;
        
        message.prefix = data.slice(position + 1, nextspace);
        position = nextspace + 1;
        
        while(data.charCodeAt(position) === 32) position++;
    }
    
    // command
    nextspace = data.indexOf(" ", position);
    if(nextspace === -1) {
        if(data.length > position) {
            message.command = data.slice(position);
            return message;
        }
        return null;
    }
    
    message.command = data.slice(position, nextspace);
    
    // params
    position = nextspace + 1;
    while(data.charCodeAt(position) === 32) position++; // 32 = ' '
    
    while(position < data.length) {
        // ':' means trailing param. Usually the chat message of a PRIVMSG
        if(data.charCodeAt(position) === 58) { // 58 = ':'
            message.params.push(data.slice(position + 1));
            break;
        }
        
        nextspace = data.indexOf(" ", position);
        if(nextspace === -1) {
            message.params.push(data.slice(position));
            break;
        }
        
        message.params.push(data.slice(position, nextspace));
        position = nextspace + 1;
        
        while(data.charCodeAt(position) === 32) position++; // 32 = ' '
    }
    return message;
}

function break_by_spaces(s) {
    if(s.length > 0) {
        let i = s.indexOf(" ");
        if(i >= 0) {
            let lhs = s.splice(0, i);
            i++;
            while(s.charCodeAt(i) === 32) i++; // charCodeAt returns NaN if out of range so no need to range check
            let rhs = s.splice(i);
            return [lhs, rhs];
        }
    }
    return [s, ""];
}

function break_by_char(s, c) {
    if(s.length > 0) {
        let i = s.indexOf(c);
        if(i >= 0) {
            let lhs = s.slice(0, i);
            let rhs = s.slice(i + 1);
            return [lhs, rhs];
        }
    }
    return [s, ""];
}

function slice_before_char(s, c) {
    if(s.length > 0) {
        let i = s.indexOf(c);
        if(i >= 0) {
            return s.slice(0, i);
        }
    }
    return s;
}