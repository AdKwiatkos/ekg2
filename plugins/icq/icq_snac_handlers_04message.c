/*
 *  (C) Copyright 2000-2001 Richard Hughes, Roland Rabien, Tristan Van de Vreede
 *  (C) Copyright 2001-2002 Jon Keating, Richard Hughes
 *  (C) Copyright 2002-2004 Martin Öberg, Sam Kothari, Robert Rainwater
 *  (C) Copyright 2004-2008 Joe Kucera
 *
 * ekg2 port:
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

SNAC_SUBHANDLER(icq_snac_message_error) {
	struct {
		uint16_t error;
	} pkt;

	if (!ICQ_UNPACK(&buf, "W", &pkt.error))
		return -1;

	debug_error("icq_snac_message_error() XXX\n");

	icq_snac_error_handler(s, "message", pkt.error);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_replyicbm) {
	string_t pkt;
	uint32_t flags;

	/* Set message parameters for all channels (imitate ICQ 6) */
	flags = 0x00000303;
#ifdef DBG_CAPHTML
	flags |= 0x00000400;
#endif
#ifdef DBG_CAPMTN
	flags |= 0x00000008;
#endif
	/* SnacCliSeticbm() */
flags = 0x0b;
	pkt = icq_pack("WIWWWWW",
		(uint32_t) 0x0001, (uint32_t) flags,		/* channel, flags */
		(uint16_t) 8000, (uint32_t) 999,		/* max-message-snac-size, max-sender-warning-level */
		(uint32_t) 999, (uint32_t) 0,			/* max-rcv-warning-level, minimum message-interval-in-secons */
		(uint32_t) 0);					/* unknown */
	icq_makesnac(s, pkt, 0x04, 0x02, 0, 0);
	icq_send_pkt(s, pkt);

flags = 0x03;
	pkt = icq_pack("WIWWWWW",
		(uint32_t) 0x0002, (uint32_t) flags,		/* channel, flags */
		(uint16_t) 8000, (uint32_t) 999,		/* max-message-snac-size, max-sender-warning-level */
		(uint32_t) 999, (uint32_t) 0,			/* max-rcv-warning-level, minimum message-interval-in-secons */
		(uint32_t) 0);					/* unknown */
	icq_makesnac(s, pkt, 0x04, 0x02, 0, 0);
	icq_send_pkt(s, pkt);

	pkt = icq_pack("WIWWWWW",
		(uint32_t) 0x0004, (uint32_t) flags,		/* channel, flags */
		(uint16_t) 8000, (uint32_t) 999,		/* max-message-snac-size, max-sender-warning-level */
		(uint32_t) 999, (uint32_t) 0,			/* max-rcv-warning-level, minimum message-interval-in-secons */
		(uint32_t) 0);					/* unknown */
	icq_makesnac(s, pkt, 0x04, 0x02, 0, 0);
	icq_send_pkt(s, pkt);

	return 0;
}

int icq_snac_message_recv_simple(session_t *s, unsigned char *buf, int len, const char *sender) {
	struct icq_tlv_list *tlvs;
	struct icq_tlv_list *tlvs_msg;
	icq_tlv_t *t;
	
	debug_function("icq_snac_message_recv_simple() from: %s leftlen: %d\n", sender, len);

	if (!(tlvs = icq_unpack_tlvs(buf, len, 0))) {
		debug("icq_snac_message_recv_simple() ignoring empty message..\n");
		return 0;
	}
	
	if (!(t = icq_tlv_get(tlvs, 0x02)) || t->type != 0x02) {
		debug_error("icq_snac_message_recv_simple() TLV(0x02) not found?\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	if (!(tlvs_msg = icq_unpack_tlvs(t->buf, t->len, 0))) {
		debug_error("icq_snac_message_recv_simple() failed to read tlv chain in message\n");
		icq_tlvs_destroy(&tlvs);
		return 1;
	}

	if ((t = icq_tlv_get(tlvs_msg, 0x501))) {
		debug("icq_snac_message_recv_simple() message has: %d caps\n", t->len);
	} else
		debug("icq_snac_message_recv_simple() no message cap\n");

	{
		/* Parse the message parts, usually only one 0x0101 TLV containing the message,
		 * but in some cases there can be more 0x0101 TLVs containing message parts in
		 * different encoding (just like the new format of Offline Messages) */ 
		struct icq_tlv_list *t;
		string_t msg = string_init(NULL);
		time_t sent;

		for (t = tlvs_msg; t; t = t->next) {
			struct {
				uint16_t encoding;
				uint16_t codepage;
				unsigned char *message;
			} t_msg;

			int t_len = t->len;

			if (t->type != 0x0101)
				continue;

			if (!icq_unpack(t->buf, &t_msg.message, &t_len, "ww", &t_msg.encoding, &t_msg.codepage))
				continue;

			debug_function("icq_snac_message_recv_simple() enc: %.4x cp: %.4x\n", t_msg.encoding, t_msg.codepage);

			switch (t_msg.encoding) {
				case 0x02:
				{
					/* XXX, UCS-2 */
					debug_error("icq_snac_message_recv_simple() message in UCS-2 !!! not supported, yet, sorry :(\n");
					icq_hexdump(DEBUG_ERROR, t_msg.message, t_len);
					break;
				}
				case 0x00:	/* US-ASCII */
				case 0x03:	/* ANSI */
				default:
				{
					string_append_n(msg, t_msg.message, t_len);
				}

				/* XXX, recode */
			}
		}

		/* XXX, check if message was recv when we was offline */

		sent = time(NULL);

		if (msg->len) {
			char *uid = icq_uid(sender);
			/* XXX, class? */
			protocol_message_emit(s, uid, NULL, msg->str, NULL, sent, EKG_MSGCLASS_CHAT, NULL, EKG_TRY_BEEP, 0);
			xfree(uid);
		}

		string_free(msg, 1);
	}

	icq_tlvs_destroy(&tlvs);
	icq_tlvs_destroy(&tlvs_msg);
	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_recv) {
	struct {
		uint32_t id1;
		uint32_t id2;
		uint16_t format;	/* 0x01 - simple message format
					   0x02 - advanced message format
					   0x04 - 'new' message format
					 */
		char *sender;
		uint16_t warning_level;	/* not used */
		uint16_t tlv_count;
	} pkt;

	if (!ICQ_UNPACK(&buf, "iiWuWW", &pkt.id1, &pkt.id2, &pkt.format, &pkt.sender, &pkt.warning_level, &pkt.tlv_count)) {
		debug_error("icq_snac_message_recv() Malformed message thru server\n");
		return -1;
	}

	debug_function("icq_snac_message_recv() from: %s id1: %.8x id2: %.8x format: %.4x warning: %.4x tlvs: %.4x\n",
				pkt.sender, pkt.id1, pkt.id2, pkt.format, pkt.warning_level, pkt.tlv_count);

	/* XXX, spamer? */
	while (pkt.tlv_count) {
		uint16_t t_type, t_len;

		/* XXX, przerobic icq_unpack_tlvs() zeby uaktualnialo buf i len!!! */

		if (!ICQ_UNPACK(&buf, "WW", &t_type, &t_len))
			return -1;
		if (len < t_len)
			return -1;

		buf += t_len; len -= t_len;

		pkt.tlv_count--;
	}

	switch (pkt.format) {
		case 0x01:
			icq_snac_message_recv_simple(s, buf, len, pkt.sender);
			break;

		case 0x02:
		case 0x04:
		default:
			debug_error("icq_snac_message_recv() unknown format message from server sender: %s\n", pkt.sender);
			/* dump message */
			icq_hexdump(DEBUG_ERROR, buf, len);
			break;
	}
	return 0;
}

SNAC_SUBHANDLER(icq_snac_message_queue) {	/* SNAC(4, 0x17) Offline Messages response */
	debug_error("icq_snac_message_queue() XXX\n");

#if MIRANDA
	offline_message_cookie *cookie;

	if (FindCookie(dwRef, NULL, (void**)&cookie))
	{
		NetLog_Server("End of offline msgs, %u received", cookie->nMessages);
		if (cookie->nMissed)
		{ // NASTY WORKAROUND!!
			// The ICQ server has a bug that causes offline messages to be received again and again when some 
			// missed message notification is present (most probably it is not processed correctly and causes
			// the server to fail the purging process); try to purge them using the old offline messages
			// protocol.  2008/05/21
			NetLog_Server("Warning: Received %u missed message notifications, trying to fix the server.", cookie->nMissed);

			icq_packet packet;
			// This will delete the messages stored on server
			serverPacketInit(&packet, 24);
			packFNACHeader(&packet, ICQ_EXTENSIONS_FAMILY, ICQ_META_CLI_REQ);
			packWord(&packet, 1);             // TLV Type
			packWord(&packet, 10);            // TLV Length
			packLEWord(&packet, 8);           // Data length
			packLEDWord(&packet, m_dwLocalUIN); // My UIN
			packLEWord(&packet, CLI_DELETE_OFFLINE_MSGS_REQ); // Ack offline msgs
			packLEWord(&packet, 0x0000);      // Request sequence number (we dont use this for now)

			sendServPacket(&packet);
		}

		ReleaseCookie(dwRef);
	}
	else
		NetLog_Server("Error: Received unexpected end of offline msgs.");
#endif
	return -3;
}

SNAC_HANDLER(icq_snac_message_handler) {
	snac_subhandler_t handler;

	switch (cmd) {
		case 0x01: handler = icq_snac_message_error; break;
		case 0x05: handler = icq_snac_message_replyicbm; break;		/* Miranda: OK */
		case 0x07: handler = icq_snac_message_recv; break;
		case 0x17: handler = icq_snac_message_queue; break;
		default:   handler = NULL; break;
	}

	if (!handler) {
		debug_error("icq_snac_message_handler() SNAC with unknown cmd: %.4x received\n", cmd);
		icq_hexdump(DEBUG_ERROR, buf, len);
	} else
		handler(s, buf, len);

	return 0;
}
