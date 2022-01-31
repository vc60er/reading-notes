# janus-gateway

## janus-videoroom

```c
/*! \brief The plugin session and callbacks interface */
struct janus_plugin {
	/*! \brief Plugin initialization/constructor
	 * @param[in] callback The callback instance the plugin can use to contact the Janus core
	 * @param[in] config_path Path of the folder where the configuration for this plugin can be found
	 * @returns 0 in case of success, a negative integer in case of error */
	int (* const init)(janus_callbacks *callback, const char *config_path);
	/*! \brief Plugin deinitialization/destructor */
	void (* const destroy)(void);

	/*! \brief Informative method to request the API version this plugin was compiled against
	 *  \note This was added in version 0.0.7 of Janus, to address changes
	 * to the API that might break existing plugin or the core itself. All
	 * plugins MUST implement this method and return JANUS_PLUGIN_API_VERSION
	 * to make this work, or they will be rejected by the core. Do NOT try
	 * to launch a <= 0.0.7 plugin on a >= 0.0.7 Janus or it will crash. */
	int (* const get_api_compatibility)(void);
	/*! \brief Informative method to request the numeric version of the plugin */
	int (* const get_version)(void);
	/*! \brief Informative method to request the string version of the plugin */
	const char *(* const get_version_string)(void);
	/*! \brief Informative method to request a description of the plugin */
	const char *(* const get_description)(void);
	/*! \brief Informative method to request the name of the plugin */
	const char *(* const get_name)(void);
	/*! \brief Informative method to request the author of the plugin */
	const char *(* const get_author)(void);
	/*! \brief Informative method to request the package name of the plugin (what will be used in web applications to refer to it) */
	const char *(* const get_package)(void);

	/*! \brief Method to create a new session/handle for a peer
	 * @param[in] handle The plugin/gateway session that will be used for this peer
	 * @param[out] error An integer that may contain information about any error */
	void (* const create_session)(janus_plugin_session *handle, int *error);
	/*! \brief Method to handle an incoming message/request from a peer
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] transaction The transaction identifier for this message/request
	 * @param[in] message The json_t object containing the message/request JSON
	 * @param[in] jsep The json_t object containing the JSEP type/SDP, if available
	 * @returns A janus_plugin_result instance that may contain a response (for immediate/synchronous replies), an ack
	 * (for asynchronously managed requests) or an error */
	struct janus_plugin_result * (* const handle_message)(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
	/*! \brief Method to handle an incoming Admin API message/request
	 * @param[in] message The json_t object containing the message/request JSON
	 * @returns A json_t instance containing the response */
	struct json_t * (* const handle_admin_message)(json_t *message);
	/*! \brief Callback to be notified when the associated PeerConnection is up and ready to be used
	 * @param[in] handle The plugin/gateway session used for this peer */
	void (* const setup_media)(janus_plugin_session *handle);
	/*! \brief Method to handle an incoming RTP packet from a peer
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] packet The RTP packet and related data */
	void (* const incoming_rtp)(janus_plugin_session *handle, janus_plugin_rtp *packet);
	/*! \brief Method to handle an incoming RTCP packet from a peer
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] packet The RTP packet and related data */
	void (* const incoming_rtcp)(janus_plugin_session *handle, janus_plugin_rtcp *packet);
	/*! \brief Method to handle incoming SCTP/DataChannel data from a peer (text only, for the moment)
	 * \note We currently only support text data, binary data will follow... please also notice that
	 * DataChannels send unterminated strings, so you'll have to terminate them with a \0 yourself to
	 * use them.
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] packet The message data and related info */
	void (* const incoming_data)(janus_plugin_session *handle, janus_plugin_data *packet);
	/*! \brief Method to be notified by the core when too many NACKs have
	 * been received or sent by Janus, and so a slow or potentially
	 * unreliable network is to be expected for this peer
	 * \note Beware that this callback may be called more than once in a row,
	 * (even though never more than once per second), until things go better for that
	 * PeerConnection. You may or may not want to handle this callback and
	 * act on it, considering you can get bandwidth information from REMB
	 * feedback sent by the peer if the browser supports it. Besides, your
	 * plugin may not have access to encoder related settings to slow down
	 * or decreae the bitrate if required after the callback is called.
	 * Nevertheless, it can be useful for debugging, or for informing your
	 * users about potential issues that may be happening media-wise.
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] uplink Whether this is related to the uplink (Janus to peer)
	 * or downlink (peer to Janus)
	 * @param[in] video Whether this is related to an audio or a video stream */
	void (* const slow_link)(janus_plugin_session *handle, gboolean uplink, gboolean video);
	/*! \brief Callback to be notified about DTLS alerts from a peer (i.e., the PeerConnection is not valid any more)
	 * @param[in] handle The plugin/gateway session used for this peer */
	void (* const hangup_media)(janus_plugin_session *handle);
	/*! \brief Method to destroy a session/handle for a peer
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[out] error An integer that may contain information about any error */
	void (* const destroy_session)(janus_plugin_session *handle, int *error);
	/*! \brief Method to get plugin-specific info of a session/handle
	 *  \note This was added in version 0.0.7 of Janus. Janus assumes
	 * the string is always allocated, so don't return constants here
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @returns A json_t object with the requested info */
	json_t *(* const query_session)(janus_plugin_session *handle);

};



/*! \brief Callbacks to contact the Janus core */
struct janus_callbacks {
	/*! \brief Callback to push events/messages to a peer
	 * @note The Janus core increases the references to both the message and jsep
	 * json_t objects. This means that you'll have to decrease your own
	 * reference yourself with a \c json_decref after calling push_event.
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] plugin The plugin instance that is sending the message/event
	 * @param[in] transaction The transaction identifier this message refers to
	 * @param[in] message The json_t object containing the JSON message
	 * @param[in] jsep The json_t object containing the JSEP type, the SDP attached to the message/event, if any (offer/answer), and whether this is an update */
	int (* const push_event)(janus_plugin_session *handle, janus_plugin *plugin, const char *transaction, json_t *message, json_t *jsep);

	/*! \brief Callback to relay RTP packets to a peer
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] packet The RTP packet and related data */
	void (* const relay_rtp)(janus_plugin_session *handle, janus_plugin_rtp *packet);
	/*! \brief Callback to relay RTCP messages to a peer
	 * @param[in] handle The plugin/gateway session that will be used for this peer
	 * @param[in] packet The RTCP packet and related data */
	void (* const relay_rtcp)(janus_plugin_session *handle, janus_plugin_rtcp *packet);
	/*! \brief Callback to relay SCTP/DataChannel messages to a peer
	 * @param[in] handle The plugin/gateway session that will be used for this peer
	 * @param[in] packet The message data and related info */
	void (* const relay_data)(janus_plugin_session *handle, janus_plugin_data *packet);

	/*! \brief Helper to ask for a keyframe via a RTCP PLI
	 * @note This is a shortcut, as it is also possible to do the same by crafting
	 * an RTCP PLI message manually, and passing it to the core via relay_rtcp
	 * @param[in] handle The plugin/gateway session that will be used for this peer */
	void (* const send_pli)(janus_plugin_session *handle);
	/*! \brief Helper to ask for a keyframe via a RTCP PLI
	 * @note This is a shortcut, as it is also possible to do the same by crafting
	 * an RTCP REMB message manually, and passing it to the core via relay_rtcp
	 * @param[in] handle The plugin/gateway session that will be used for this peer
	 * @param[in] bitrate The bitrate value to send in the REMB message */
	void (* const send_remb)(janus_plugin_session *handle, guint32 bitrate);

	/*! \brief Callback to ask the core to close a WebRTC PeerConnection
	 * \note A call to this method will result in the core invoking the hangup_media
	 * callback on this plugin when done
	 * @param[in] handle The plugin/gateway session that the PeerConnection is related to */
	void (* const close_pc)(janus_plugin_session *handle);
	/*! \brief Callback to ask the core to get rid of a plugin/gateway session
	 * \note A call to this method will result in the core invoking the destroy_session
	 * callback on this plugin when done
	 * @param[in] handle The plugin/gateway session to get rid of */
	void (* const end_session)(janus_plugin_session *handle);

	/*! \brief Callback to check whether the event handlers mechanism is enabled
	 * @returns TRUE if it is, FALSE if it isn't (which means notify_event should NOT be called) */
	gboolean (* const events_is_enabled)(void);
	/*! \brief Callback to notify an event to the registered and subscribed event handlers
	 * \note Don't unref the event object, the core will do that for you
	 * @param[in] plugin The plugin originating the event
	 * @param[in] handle The plugin/gateway session originating the event, if any
	 * @param[in] event The event to notify as a Jansson json_t object */
	void (* const notify_event)(janus_plugin *plugin, janus_plugin_session *handle, json_t *event);

	/*! \brief Method to check whether a signed token is valid
	 * \note accepts only tokens with the plugin identifier as realm
	 * @param[in] token The token to validate
	 * @returns TRUE if the signature is valid and not expired, FALSE otherwise */
	gboolean (* const auth_is_signature_valid)(janus_plugin *plugin, const char *token);
	/*! \brief Method to verify a signed token grants access to a descriptor
	 * \note accepts only tokens with the plugin identifier as realm
	 * @param[in] token The token to validate
	 * @param[in] desc The descriptor to search for
	 * @returns TRUE if the token is valid, not expired and contains the descriptor, FALSE otherwise */
	gboolean (* const auth_signature_contains)(janus_plugin *plugin, const char *token, const char *descriptor);
};


/*! \brief Plugin-Gateway session mapping */
struct janus_plugin_session {
	/*! \brief Opaque pointer to the Janus core-level handle */
	void *gateway_handle;
	/*! \brief Opaque pointer to the plugin session */
	void *plugin_handle;
	/*! \brief Whether this mapping has been stopped definitely or not: if so,
	 * the plugin shouldn't make use of it anymore */
	volatile gint stopped;
	/*! \brief Reference counter for this instance */
	janus_refcount ref;
};


typedef struct janus_videoroom_session {
	janus_plugin_session *handle;
	gint64 sdp_sessid;
	gint64 sdp_version;
	janus_videoroom_p_type participant_type;
	gpointer participant;
	gboolean started;
	gboolean stopping;
	volatile gint hangingup;
	volatile gint destroyed;
	janus_mutex mutex;
	janus_refcount ref;
} janus_videoroom_session;

typedef struct janus_videoroom {
	guint64 room_id;			/* Unique room ID (when using integers) */
	gchar *room_id_str;			/* Unique room ID (when using strings) */
	gchar *room_name;			/* Room description */
	gchar *room_secret;			/* Secret needed to manipulate (e.g., destroy) this room */
	gchar *room_pin;			/* Password needed to join this room, if any */
	gboolean is_private;		/* Whether this room is 'private' (as in hidden) or not */
	gboolean require_pvtid;		/* Whether subscriptions in this room require a private_id */
	int max_publishers;			/* Maximum number of concurrent publishers */
	uint32_t bitrate;			/* Global bitrate limit */
	gboolean bitrate_cap;		/* Whether the above limit is insormountable */
	uint16_t fir_freq;			/* Regular FIR frequency (0=disabled) */
	janus_audiocodec acodec[3];	/* Audio codec(s) to force on publishers */
	janus_videocodec vcodec[3];	/* Video codec(s) to force on publishers */
	gboolean do_opusfec;		/* Whether inband FEC must be negotiated (note: only available for Opus) */
	gboolean do_svc;			/* Whether SVC must be done for video (note: only available for VP9 right now) */
	gboolean audiolevel_ext;	/* Whether the ssrc-audio-level extension must be negotiated or not for new publishers */
	gboolean audiolevel_event;	/* Whether to emit event to other users about audiolevel */
	int audio_active_packets;	/* Amount of packets with audio level for checkup */
	int audio_level_average;	/* Average audio level */
	gboolean videoorient_ext;	/* Whether the video-orientation extension must be negotiated or not for new publishers */
	gboolean playoutdelay_ext;	/* Whether the playout-delay extension must be negotiated or not for new publishers */
	gboolean transport_wide_cc_ext;	/* Whether the transport wide cc extension must be negotiated or not for new publishers */
	gboolean record;			/* Whether the feeds from publishers in this room should be recorded */
	char *rec_dir;				/* Where to save the recordings of this room, if enabled */
	GHashTable *participants;	/* Map of potential publishers (we get subscribers from them) */
	GHashTable *private_ids;	/* Map of existing private IDs */
	volatile gint destroyed;	/* Whether this room has been destroyed */
	gboolean check_allowed;		/* Whether to check tokens when participants join (see below) */
	GHashTable *allowed;		/* Map of participants (as tokens) allowed to join */
	gboolean notify_joining;	/* Whether an event is sent to notify all participants if a new participant joins the room */
	janus_mutex mutex;			/* Mutex to lock this room instance */
	janus_refcount ref;			/* Reference counter for this room */
} janus_videoroom;


typedef struct janus_videoroom_publisher {
	janus_videoroom_session *session;
	janus_videoroom *room;	/* Room */
	guint64 room_id;	/* Unique room ID */
	gchar *room_id_str;	/* Unique room ID (when using strings) */
	guint64 user_id;	/* Unique ID in the room */
	gchar *user_id_str;	/* Unique ID in the room (when using strings) */
	guint32 pvt_id;		/* This is sent to the publisher for mapping purposes, but shouldn't be shared with others */
	gchar *display;		/* Display name (just for fun) */
	gchar *sdp;			/* The SDP this publisher negotiated, if any */
	gboolean audio, video, data;		/* Whether audio, video and/or data is going to be sent by this publisher */
	janus_audiocodec acodec;	/* Audio codec this publisher is using */
	janus_videocodec vcodec;	/* Video codec this publisher is using */
	guint32 audio_pt;		/* Audio payload type (Opus) */
	guint32 video_pt;		/* Video payload type (depends on room configuration) */
	guint32 audio_ssrc;		/* Audio SSRC of this publisher */
	guint32 video_ssrc;		/* Video SSRC of this publisher */
	gboolean do_opusfec;	/* Whether this publisher is sending inband Opus FEC */
	uint32_t ssrc[3];		/* Only needed in case VP8 (or H.264) simulcasting is involved */
	char *rid[3];			/* Only needed if simulcasting is rid-based */
	int rid_extmap_id;		/* rid extmap ID */
	int framemarking_ext_id;			/* Frame marking extmap ID */
	guint8 audio_level_extmap_id;		/* Audio level extmap ID */
	guint8 video_orient_extmap_id;		/* Video orientation extmap ID */
	guint8 playout_delay_extmap_id;		/* Playout delay extmap ID */
	gboolean audio_active;
	gboolean video_active;
	int audio_dBov_level;		/* Value in dBov of the audio level (last value from extension) */
	int audio_active_packets;	/* Participant's number of audio packets to accumulate */
	int audio_dBov_sum;			/* Participant's accumulated dBov value for audio level*/
	gboolean talking;			/* Whether this participant is currently talking (uses audio levels extension) */
	gboolean data_active;
	gboolean firefox;	/* We send Firefox users a different kind of FIR */
	uint32_t bitrate;
	gint64 remb_startup;/* Incremental changes on REMB to reach the target at startup */
	gint64 remb_latest;	/* Time of latest sent REMB (to avoid flooding) */
	gint64 fir_latest;	/* Time of latest sent FIR (to avoid flooding) */
	gint fir_seq;		/* FIR sequence number */
	gboolean recording_active;	/* Whether this publisher has to be recorded or not */
	gchar *recording_base;	/* Base name for the recording (e.g., /path/to/filename, will generate /path/to/filename-audio.mjr and/or /path/to/filename-video.mjr */
	janus_recorder *arc;	/* The Janus recorder instance for this publisher's audio, if enabled */
	janus_recorder *vrc;	/* The Janus recorder instance for this user's video, if enabled */
	janus_recorder *drc;	/* The Janus recorder instance for this publisher's data, if enabled */
	janus_rtp_switching_context rec_ctx;
	janus_rtp_simulcasting_context rec_simctx;
	janus_mutex rec_mutex;	/* Mutex to protect the recorders from race conditions */
	GSList *subscribers;	/* Subscriptions to this publisher (who's watching this publisher)  */
	GSList *subscriptions;	/* Subscriptions this publisher has created (who this publisher is watching) */
	janus_mutex subscribers_mutex;
	GHashTable *rtp_forwarders;
	GHashTable *srtp_contexts;
	janus_mutex rtp_forwarders_mutex;
	int udp_sock; /* The udp socket on which to forward rtp packets */
	gboolean kicked;	/* Whether this participant has been kicked */
	volatile gint destroyed;
	janus_refcount ref;
} janus_videoroom_publisher;


typedef struct janus_videoroom_subscriber {
	janus_videoroom_session *session;
	janus_videoroom *room;	/* Room */
	guint64 room_id;		/* Unique room ID */
	gchar *room_id_str;		/* Unique room ID (when using strings) */
	janus_videoroom_publisher *feed;	/* Participant this subscriber is subscribed to */
	gboolean close_pc;		/* Whether we should automatically close the PeerConnection when the publisher goes away */
	guint32 pvt_id;			/* Private ID of the participant that is subscribing (if available/provided) */
	janus_sdp *sdp;			/* Offer we sent this listener (may be updated within renegotiations) */
	janus_rtp_switching_context context;	/* Needed in case there are publisher switches on this subscriber */
	janus_rtp_simulcasting_context sim_context;
	janus_vp8_simulcast_context vp8_context;
	gboolean audio, video, data;		/* Whether audio, video and/or data must be sent to this subscriber */
	/* As above, but can't change dynamically (says whether something was negotiated at all in SDP) */
	gboolean audio_offered, video_offered, data_offered;
	gboolean paused;
	gboolean kicked;	/* Whether this subscription belongs to a participant that has been kicked */
	/* The following are only relevant if we're doing VP9 SVC, and are not to be confused with plain
	 * simulcast, which has similar info (substream/templayer) but in a completely different context */
	int spatial_layer, target_spatial_layer;
	gint64 last_spatial_layer[3];
	int temporal_layer, target_temporal_layer;
	volatile gint destroyed;
	janus_refcount ref;
} janus_videoroom_subscriber;







```

```c
static GHashTable *sessions;


static GAsyncQueue *messages = NULL;
static janus_callbacks *gateway = NULL;



```

```c
int janus_videoroom_init(janus_callbacks *callback, const char *config_path);
	// 注册回调
	// 加载房间
	// 启动处理线程
	// thread
	static void *janus_videoroom_handler(void *data);
	// thread
	static void *janus_videoroom_rtp_forwarder_rtcp_thread(void *data) {


void janus_videoroom_create_session(janus_plugin_session *handle, int *error);

struct janus_plugin_result *janus_videoroom_handle_message(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
	janus_videoroom_process_synchronous_request
		} else if(!strcasecmp(request_text, "join") 
			|| !strcasecmp(request_text, "joinandconfigure")
			|| !strcasecmp(request_text, "configure") 
			|| !strcasecmp(request_text, "publish") 
			|| !strcasecmp(request_text, "unpublish")
			|| !strcasecmp(request_text, "start") 
			|| !strcasecmp(request_text, "pause") 
			|| !strcasecmp(request_text, "switch")
			|| !strcasecmp(request_text, "leave")) {



json_t *janus_videoroom_handle_admin_message(json_t *message)
	janus_videoroom_process_synchronous_request


static json_t *janus_videoroom_process_synchronous_request(janus_videoroom_session *session, json_t *message) {
	if(!strcasecmp(request_text, "create")) {
	} else if(!strcasecmp(request_text, "edit")) {
	} else if(!strcasecmp(request_text, "destroy")) {
	} else if(!strcasecmp(request_text, "list")) {
	} else if(!strcasecmp(request_text, "rtp_forward")) {
	} else if(!strcasecmp(request_text, "stop_rtp_forward")) {
	} else if(!strcasecmp(request_text, "exists")) {
	} else if(!strcasecmp(request_text, "allowed")) {
	} else if(!strcasecmp(request_text, "kick")) {
	} else if(!strcasecmp(request_text, "listparticipants")) {
	} else if(!strcasecmp(request_text, "listforwarders")) {




void janus_videoroom_setup_media(janus_plugin_session *handle)
	if(session->participant) {
		if(session->participant_type == janus_videoroom_p_type_publisher) {
							janus_videoroom_notify_participants(participant, pub);

				gateway->notify_event(&janus_videoroom_plugin, session->handle, info);
		} else if(session->participant_type == janus_videoroom_p_type_subscriber) {
								janus_videoroom_reqpli(p, "New subscriber available");

				gateway->notify_event(&janus_videoroom_plugin, session->handle, info);
		}
	}

void janus_videoroom_incoming_rtp(janus_plugin_session *handle, janus_plugin_rtp *pkt) {
	// notify_talk_event
	/* Check if we're simulcasting, and if so, keep track of the "layer" */
	/* Forward RTP to the appropriate port for the rtp_forwarders associated with this publisher, if there are any */
	/* Save the frame if we're recording */
	/* Done, relay it */
		g_slist_foreach(participant->subscribers, janus_videoroom_relay_rtp_packet, &packet);
	/* Check if we need to send any REMB, FIR or PLI back to this publisher */



	/*! \brief Method to handle an incoming RTCP packet from a peer
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] packet The RTP packet and related data */
static void janus_videoroom_relay_rtp_packet(gpointer data, gpointer user_data) {
			/* Check if there's any SVC info to take into account */
			/* Handle simulcast: make sure we have a payload to work with */
				gateway->relay_rtp(session->handle, &rtp);




	/*! \brief Method to handle incoming SCTP/DataChannel data from a peer (text only, for the moment)
	 * \note We currently only support text data, binary data will follow... please also notice that
	 * DataChannels send unterminated strings, so you'll have to terminate them with a \0 yourself to
	 * use them.
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] packet The message data and related info */
void janus_videoroom_incoming_rtcp(janus_plugin_session *handle, janus_plugin_rtcp *packet) {
		/* A subscriber sent some RTCP, check what it is and if we need to forward it to the publisher */	
			/* We got a FIR or PLI, forward a PLI it to the publisher */

	/*! \brief Method to handle incoming SCTP/DataChannel data from a peer (text only, for the moment)
	 * \note We currently only support text data, binary data will follow... please also notice that
	 * DataChannels send unterminated strings, so you'll have to terminate them with a \0 yourself to
	 * use them.
	 * @param[in] handle The plugin/gateway session used for this peer
	 * @param[in] packet The message data and related info */

void janus_videoroom_incoming_data(janus_plugin_session *handle, janus_plugin_data *packet) {


	/*! \brief Method to be notified by the core when too many NACKs have
	 * been received or sent by Janus, and so a slow or potentially
	 * unreliable network is to be expected for this peer
	 */
void janus_videoroom_slow_link(janus_plugin_session *handle, int uplink, int video) {


janus_videoroom_hangup_media
janus_videoroom_destroy_session
janus_videoroom_query_session


```







### 修改
1. 订阅方式修改
2. 动态创建房间 ?

	gboolean do_opusfec;		/* Whether inband FEC must be negotiated (note: only available for Opus) */
声音level
声音频谱
	uint32_t ssrc;


SSRC
SVC


