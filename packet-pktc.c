/* packet-pktc.c
 * Declarations of routines for PKTC PacketCable packet disassembly
 * Ronnie Sahlberg 2004
 * See the spec: PKT-SP-SEC-I10-040113.pdf
 *
 * $Id: packet-pktc.c,v 1.2 2004/05/18 11:08:26 sahlberg Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
 * Copyright 1998 Gerald Combs
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <epan/packet.h>
#include "packet-pktc.h"
#include "packet-kerberos.h"

#define PKTC_PORT	1293

static int proto_pktc = -1;
static gint hf_pktc_app_spec_data = -1;
static gint hf_pktc_list_of_ciphersuites = -1;
static gint hf_pktc_list_of_ciphersuites_len = -1;
static gint hf_pktc_kmmid = -1;
static gint hf_pktc_doi = -1;
static gint hf_pktc_version_major = -1;
static gint hf_pktc_version_minor = -1;
static gint hf_pktc_server_nonce = -1;
static gint hf_pktc_snmpEngineID_len = -1;
static gint hf_pktc_snmpEngineID = -1;
static gint hf_pktc_snmpEngineID_boots = -1;
static gint hf_pktc_snmpEngineID_time = -1;
static gint hf_pktc_usmUserName_len = -1;
static gint hf_pktc_usmUserName = -1;
static gint hf_pktc_snmpAuthenticationAlgorithm = -1;
static gint hf_pktc_snmpEncryptionTransformID = -1;
static gint hf_pktc_reestablish_flag = -1;
static gint hf_pktc_ack_required_flag = -1;
static gint hf_pktc_sha1_mac = -1;
static gint hf_pktc_sec_param_lifetime = -1;
static gint hf_pktc_grace_period = -1;

static gint ett_pktc = -1;
static gint ett_pktc_app_spec_data = -1;
static gint ett_pktc_list_of_ciphersuites = -1;

#define KMMID_WAKEUP		0x01
#define KMMID_AP_REQUEST	0x02
#define KMMID_AP_REPLY		0x03
#define KMMID_SEC_PARAM_REC	0x04
#define KMMID_REKEY		0x05
#define KMMID_ERROR_REPLY	0x06
static const value_string kmmid_types[] = {
    { KMMID_WAKEUP		, "Wake Up" },
    { KMMID_AP_REQUEST		, "AP Request" },
    { KMMID_AP_REPLY		, "AP Reply" },
    { KMMID_SEC_PARAM_REC	, "Security Parameter Recovered" },
    { KMMID_REKEY		, "Rekey" },
    { KMMID_ERROR_REPLY		, "Error Reply" },
    { 0, NULL }
};

#define DOI_IPSEC	1
#define DOI_SNMPv3	2
static const value_string doi_types[] = {
    { DOI_IPSEC		, "IPSec" },
    { DOI_SNMPv3	, "SNMPv3" },
    { 0, NULL }
};


static const value_string snmpAlgorithmIdentifiers_vals[] = {
    { 0x21		, "MD5-HMAC" },
    { 0x22		, "SHA1-HMAC" },
    { 0	, NULL }
};
static const value_string snmpEncryptionTransformID_vals[] = {
    { 0x20		, "SNMPv3 NULL (no encryption)" },
    { 0x21		, "SNMPv3 DES" },
    { 0	, NULL }
};

static int
dissect_pktc_app_specific_data(packet_info *pinfo _U_, proto_tree *parent_tree, tvbuff_t *tvb, int offset, guint8 doi, guint8 kmmid)
{
    int old_offset=offset;
    proto_tree *tree = NULL;
    proto_item *item = NULL;
    guint8 len;

    if (parent_tree) {
        item = proto_tree_add_item(parent_tree, hf_pktc_app_spec_data, tvb, offset, -1, FALSE);
        tree = proto_item_add_subtree(item, ett_pktc_app_spec_data);
    }

    switch(doi){
    case DOI_SNMPv3:
        switch(kmmid){
        /* we dont distinguish between manager and agent engineid.
           feel free to add separation for this if it is imporant enough
           for you. */
        case KMMID_AP_REQUEST:
        case KMMID_AP_REPLY:
            /* snmpEngineID Length */
            len=tvb_get_guint8(tvb, offset);
            proto_tree_add_uint(tree, hf_pktc_snmpEngineID_len, tvb, offset, 1, len);
            offset+=1;

            /* snmpEngineID */
            proto_tree_add_item(tree, hf_pktc_snmpEngineID, tvb, offset, len, FALSE);
            offset+=len;

            /* boots */
            proto_tree_add_item(tree, hf_pktc_snmpEngineID_boots, tvb, offset, 4, FALSE);
            offset+=4;

            /* time */
            proto_tree_add_item(tree, hf_pktc_snmpEngineID_time, tvb, offset, 4, FALSE);
            offset+=4;

            /* usmUserName Length */
            len=tvb_get_guint8(tvb, offset);
            proto_tree_add_uint(tree, hf_pktc_usmUserName_len, tvb, offset, 1, len);
            offset+=1;

            /* usmUserName */
            proto_tree_add_item(tree, hf_pktc_usmUserName, tvb, offset, len, FALSE);
            offset+=len;

            break;
        default:
            proto_tree_add_text(tree, tvb, offset, 1, "Dont know how to parse this type of KMMID yet");
            tvb_get_guint8(tvb, 9999); /* bail out and inform user we cant dissect the packet */
        };
        break;
    default:
        proto_tree_add_text(tree, tvb, offset, 1, "Dont know how to parse this type of DOI yet");
        tvb_get_guint8(tvb, 9999); /* bail out and inform user we cant dissect the packet */
    }

    proto_item_set_len(item, old_offset-offset);
    return offset;
}

static int
dissect_pktc_list_of_ciphersuites(packet_info *pinfo _U_, proto_tree *parent_tree, tvbuff_t *tvb, int offset, guint8 doi)
{
    int old_offset=offset;
    proto_tree *tree = NULL;
    proto_item *item = NULL;
    guint8 len, i;

    if (parent_tree) {
        item = proto_tree_add_item(parent_tree, hf_pktc_list_of_ciphersuites, tvb, offset, -1, FALSE);
        tree = proto_item_add_subtree(item, ett_pktc_list_of_ciphersuites);
    }


    /* key management message id */
    len=tvb_get_guint8(tvb, offset);
    proto_tree_add_uint(tree, hf_pktc_list_of_ciphersuites_len, tvb, offset, 1, len);
    offset+=1;

    for(i=0;i<len;i++){
        switch(doi){
        case DOI_SNMPv3:
            /* authentication algorithm */
            proto_tree_add_item(tree, hf_pktc_snmpAuthenticationAlgorithm, tvb, offset, 1, FALSE);
            offset+=1;

            /* encryption transform id */
            proto_tree_add_item(tree, hf_pktc_snmpEncryptionTransformID, tvb, offset, 1, FALSE);
            offset+=1;
            break;
        default:
            proto_tree_add_text(tree, tvb, offset, 1, "Dont know how to parse this type of Algorithm Identifier yet");
            tvb_get_guint8(tvb, 9999); /* bail out and inform user we cant dissect the packet */
        }

    }

    proto_item_set_len(item, old_offset-offset);
    return offset;
}

static int
dissect_pktc_ap_request(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb, int offset, guint8 doi)
{
    tvbuff_t *pktc_tvb;
    guint32 snonce;

    /* AP Request  kerberos blob */
    pktc_tvb = tvb_new_subset(tvb, offset, -1, -1); 
    offset += dissect_kerberos_main(pktc_tvb, pinfo, tree, FALSE);

    /* Server Nonce */
    snonce=tvb_get_ntohl(tvb, offset);
    proto_tree_add_uint(tree, hf_pktc_server_nonce, tvb, offset, 4, snonce);
    offset+=4;

    /* app specific data */
    offset=dissect_pktc_app_specific_data(pinfo, tree, tvb, offset, doi, KMMID_AP_REQUEST);

    /* list of ciphersuites */
    offset=dissect_pktc_list_of_ciphersuites(pinfo, tree, tvb, offset, doi);

    /* re-establish flag */
    proto_tree_add_item(tree, hf_pktc_reestablish_flag, tvb, offset, 1, FALSE);
    offset+=1;

    /* sha1-mac */
    proto_tree_add_item(tree, hf_pktc_sha1_mac, tvb, offset, 20, FALSE);
    offset+=20;

    return offset;
}

static int
dissect_pktc_ap_reply(packet_info *pinfo, proto_tree *tree, tvbuff_t *tvb, int offset, guint8 doi)
{
    tvbuff_t *pktc_tvb;

    /* AP Reply  kerberos blob */
    pktc_tvb = tvb_new_subset(tvb, offset, -1, -1); 
    offset += dissect_kerberos_main(pktc_tvb, pinfo, tree, FALSE);

    /* app specific data */
    offset=dissect_pktc_app_specific_data(pinfo, tree, tvb, offset, doi, KMMID_AP_REPLY);

    /* selected ciphersuite */
    offset=dissect_pktc_list_of_ciphersuites(pinfo, tree, tvb, offset, doi);

    /* sec param lifetime */
    proto_tree_add_item(tree, hf_pktc_sec_param_lifetime, tvb, offset, 4, FALSE);
    offset+=4;

    /* grace period */
    proto_tree_add_item(tree, hf_pktc_grace_period, tvb, offset, 4, FALSE);
    offset+=4;

    /* re-establish flag */
    proto_tree_add_item(tree, hf_pktc_reestablish_flag, tvb, offset, 1, FALSE);
    offset+=1;

    /* ack required flag */
    proto_tree_add_item(tree, hf_pktc_ack_required_flag, tvb, offset, 1, FALSE);
    offset+=1;

    /* sha1-mac */
    proto_tree_add_item(tree, hf_pktc_sha1_mac, tvb, offset, 20, FALSE);
    offset+=20;

    return offset;
}

static void
dissect_pktc(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
    guint8 kmmid, doi, version;
    int offset=0;
    proto_tree *pktc_tree = NULL;
    proto_item *item = NULL;

    if (check_col(pinfo->cinfo, COL_PROTOCOL))
        col_set_str(pinfo->cinfo, COL_PROTOCOL, "PKTC");

    if (tree) {
        item = proto_tree_add_item(tree, proto_pktc, tvb, 0, 3, FALSE);
        pktc_tree = proto_item_add_subtree(item, ett_pktc);
    }

    /* key management message id */
    kmmid=tvb_get_guint8(tvb, offset);
    proto_tree_add_uint(pktc_tree, hf_pktc_kmmid, tvb, offset, 1, kmmid);
    offset+=1;

    /* domain of interpretation */
    doi=tvb_get_guint8(tvb, offset);
    proto_tree_add_uint(pktc_tree, hf_pktc_doi, tvb, offset, 1, doi);
    offset+=1;
    
    /* version */
    version=tvb_get_guint8(tvb, offset);
    proto_tree_add_uint(pktc_tree, hf_pktc_version_major, tvb, offset, 1, (version>>4)&0x0f);
    proto_tree_add_uint(pktc_tree, hf_pktc_version_minor, tvb, offset, 1, (version)&0x0f);
    offset+=1;

    switch(kmmid){
    case KMMID_AP_REQUEST:
        offset=dissect_pktc_ap_request(pinfo, pktc_tree, tvb, offset, doi);
        break;
    case KMMID_AP_REPLY:
        offset=dissect_pktc_ap_reply(pinfo, pktc_tree, tvb, offset, doi);
        break;
    };
}

void
proto_register_pktc(void)
{
    static hf_register_info hf[] = {
	{ &hf_pktc_kmmid, {
	    "Key Management Message ID", "pktc.kmmid", FT_UINT8, BASE_HEX,
	    VALS(kmmid_types), 0, "Key Management Message ID", HFILL }},
	{ &hf_pktc_doi, {
	    "Domain of Interpretation", "pktc.doi", FT_UINT8, BASE_DEC,
	    VALS(doi_types), 0, "Domain of Interpretation", HFILL }},
	{ &hf_pktc_version_major, {
	    "Major version", "pktc.version.major", FT_UINT8, BASE_DEC,
	    NULL, 0, "Major version of PKTC", HFILL }},
	{ &hf_pktc_version_minor, {
	    "Minor version", "pktc.version.minor", FT_UINT8, BASE_DEC,
	    NULL, 0, "Minor version of PKTC", HFILL }},
	{ &hf_pktc_server_nonce, {
	    "Server Nonce", "pktc.server_nonce", FT_UINT32, BASE_HEX,
	    NULL, 0, "Server Nonce random number", HFILL }},
	{ &hf_pktc_app_spec_data, {
	    "Application Specific data", "pktc.app_spec_data", FT_NONE, BASE_HEX,
	    NULL, 0, "KMMID/DOI application specific data", HFILL }},
	{ &hf_pktc_list_of_ciphersuites, {
	    "List of Ciphersuites", "pktc.list_of_ciphersuites", FT_NONE, BASE_HEX,
	    NULL, 0, "List of Ciphersuites", HFILL }},
	{ &hf_pktc_list_of_ciphersuites_len, {
	    "Number of Ciphersuites", "pktc.list_of_ciphersuites.len", FT_UINT8, BASE_DEC,
	    NULL, 0, "Number of Ciphersuites", HFILL }},
	{ &hf_pktc_snmpEngineID_len, {
	    "Engine ID Length", "pktc.EngineID.len", FT_UINT8, BASE_DEC,
	    NULL, 0, "Length of SNMP Engine ID", HFILL }},
	{ &hf_pktc_snmpAuthenticationAlgorithm, {
	    "snmpAuthentication Algorithm", "pktc.snmpAuthenticationAlgorithm", FT_UINT8, BASE_DEC,
	    VALS(snmpAlgorithmIdentifiers_vals), 0, "snmpAuthentication Algorithm", HFILL }},
	{ &hf_pktc_snmpEncryptionTransformID, {
	    "snmpEncryption Transform ID", "pktc.snmpEncryptionTransformID", FT_UINT8, BASE_DEC,
	    VALS(snmpEncryptionTransformID_vals), 0, "snmpEncryption Transform ID", HFILL }},
	{ &hf_pktc_snmpEngineID, {
	    "Engine ID", "pktc.EngineID", FT_BYTES, BASE_HEX,
	    NULL, 0, "SNMP Engine ID", HFILL }},
	{ &hf_pktc_snmpEngineID_boots, {
	    "Engine ID Boots", "pktc.EngineID.boots", FT_UINT32, BASE_HEX,
	    NULL, 0, "SNMP Engine ID Boots", HFILL }},
	{ &hf_pktc_snmpEngineID_time, {
	    "Engine ID Time", "pktc.EngineID.time", FT_UINT32, BASE_HEX,
	    NULL, 0, "SNMP Engine ID Time", HFILL }},
	{ &hf_pktc_usmUserName_len, {
	    "usmUserName Length", "pktc.usmUserName.len", FT_UINT8, BASE_DEC,
	    NULL, 0, "Length of usmUserName", HFILL }},
	{ &hf_pktc_usmUserName, {
	    "usmUserName", "pktc.usmUserName", FT_STRING, BASE_DEC,
	    NULL, 0, "usmUserName", HFILL }},
	{ &hf_pktc_reestablish_flag, {
	    "Re-establish Flag", "pktc.reestablish_flag", FT_UINT8, BASE_DEC,
	    NULL, 0, "Re-establish Flag", HFILL }},
	{ &hf_pktc_ack_required_flag, {
	    "ACK Required Flag", "pktc.ack_required_flag", FT_UINT8, BASE_DEC,
	    NULL, 0, "ACK Required Flag", HFILL }},
	{ &hf_pktc_sha1_mac, {
	    "SHA1 MAC", "pktc.sha1_mac", FT_BYTES, BASE_HEX,
	    NULL, 0, "SHA1 MAC", HFILL }},
	{ &hf_pktc_sec_param_lifetime, {
	    "Security Parameter Lifetime", "pktc.sec_param_lifetime", FT_UINT32, BASE_DEC,
	    NULL, 0, "Lifetime in seconds of security parameter", HFILL }},
	{ &hf_pktc_grace_period, {
	    "Grace Period", "pktc.grace_period", FT_UINT32, BASE_DEC,
	    NULL, 0, "Grace Period in seconds", HFILL }},
    };
    static gint *ett[] = {
        &ett_pktc,
        &ett_pktc_app_spec_data,
        &ett_pktc_list_of_ciphersuites,
    };

    proto_pktc = proto_register_protocol("PacketCable",
	"PKTC", "pktc");
    proto_register_field_array(proto_pktc, hf, array_length(hf));
    proto_register_subtree_array(ett, array_length(ett));
}

void
proto_reg_handoff_pktc(void)
{
    dissector_handle_t pktc_handle;

    pktc_handle = create_dissector_handle(dissect_pktc, proto_pktc);
    dissector_add("udp.port", PKTC_PORT, pktc_handle);
}
