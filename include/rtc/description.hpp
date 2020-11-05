/**
 * Copyright (c) 2019-2020 Paul-Louis Ageneau
 * Copyright (c) 2020 Staz Modrzynski
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef RTC_DESCRIPTION_H
#define RTC_DESCRIPTION_H

#include "candidate.hpp"
#include "include.hpp"

#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace rtc {

class Description {
public:
	enum class Type { Unspec, Offer, Answer, Pranswer, Rollback };
	enum class Role { ActPass, Passive, Active };
	enum class Direction { SendOnly, RecvOnly, SendRecv, Inactive, Unknown };

	Description(const string &sdp, const string &typeString = "");
	Description(const string &sdp, Type type);
	Description(const string &sdp, Type type, Role role);

	Type type() const;
	string typeString() const;
	Role role() const;
	string bundleMid() const;
	std::optional<string> iceUfrag() const;
	std::optional<string> icePwd() const;
	std::optional<string> fingerprint() const;
	bool ended() const;

	void hintType(Type type);
	void setFingerprint(string fingerprint);

	void addCandidate(Candidate candidate);
	void addCandidates(std::vector<Candidate> candidates);
	void endCandidates();
	std::vector<Candidate> extractCandidates();

	operator string() const;
	string generateSdp(string_view eol) const;
	string generateApplicationSdp(string_view eol) const;

	class Entry {
	public:
		virtual ~Entry() = default;

		virtual string type() const { return mType; }
		virtual string description() const { return mDescription; }
		virtual string mid() const { return mMid; }
		Direction direction() const { return mDirection; }
		void setDirection(Direction dir);

		operator string() const;
		string generateSdp(string_view eol, string_view addr, string_view port) const;

		virtual void parseSdpLine(string_view line);


	protected:
		Entry(const string &mline, string mid, Direction dir = Direction::Unknown);
		virtual string generateSdpLines(string_view eol) const;

		std::vector<string> mAttributes;

	private:
		string mType;
		string mDescription;
		string mMid;
		Direction mDirection;
	};

	struct Application : public Entry {
	public:
		Application(string mid = "data");
		virtual ~Application() = default;

		string description() const override;
		Application reciprocate() const;

		void setSctpPort(uint16_t port) { mSctpPort = port; }
		void hintSctpPort(uint16_t port) { mSctpPort = mSctpPort.value_or(port); }
		void setMaxMessageSize(size_t size) { mMaxMessageSize = size; }

		std::optional<uint16_t> sctpPort() const { return mSctpPort; }
		std::optional<size_t> maxMessageSize() const { return mMaxMessageSize; }

		virtual void parseSdpLine(string_view line) override;

	private:
		virtual string generateSdpLines(string_view eol) const override;

		std::optional<uint16_t> mSctpPort;
		std::optional<size_t> mMaxMessageSize;
	};

	// Media (non-data)
	class Media : public Entry {
	public:
		Media(const string &sdp);
		Media(const string &mline, string mid, Direction dir = Direction::SendOnly);
		virtual ~Media() = default;

		string description() const override;
		Media reciprocate() const;

        void removeFormat(const string &fmt);

        void addSSRC(uint32_t ssrc, std::string name);
        void addSSRC(uint32_t ssrc);
        void replaceSSRC(uint32_t oldSSRC, uint32_t ssrc, string name);
        bool hasSSRC(uint32_t ssrc);
        std::vector<uint32_t> getSSRCs();

		void setBitrate(int bitrate);
		int getBitrate() const;

		bool hasPayloadType(int payloadType) const;

		virtual void parseSdpLine(string_view line) override;

        struct RTPMap {
            RTPMap(string_view mline);

            void removeFB(const string &string);
            void addFB(const string &string);
            void addAttribute(std::string attr) {
                fmtps.emplace_back(attr);
            }


            int pt;
            string format;
            int clockRate;
            string encParams;

            std::vector<string> rtcpFbs;
            std::vector<string> fmtps;
        };

	private:
		virtual string generateSdpLines(string_view eol) const override;

		int mBas = -1;

		Media::RTPMap &getFormat(int fmt);
		Media::RTPMap &getFormat(const string &fmt);

		std::map<int, RTPMap> mRtpMap;
		std::vector<uint32_t> mSsrcs;

	public:
        void addRTPMap(const RTPMap& map);

    };

	class Audio : public Media {
	public:
		Audio(string mid = "audio", Direction dir = Direction::SendOnly);

        void addAudioCodec(int payloadType, const string &codec);
        void addOpusCodec(int payloadType);
	};

	class Video : public Media {
	public:
		Video(string mid = "video", Direction dir = Direction::SendOnly);

        void addVideoCodec(int payloadType, const string &codec);
        void addH264Codec(int payloadType);
        void addVP8Codec(int payloadType);
        void addVP9Codec(int payloadType);
	};

	bool hasApplication() const;
	bool hasAudioOrVideo() const;
	bool hasMid(string_view mid) const;

	int addMedia(Media media);
	int addMedia(Application application);
	int addApplication(string mid = "data");
	int addVideo(string mid = "video", Direction dir = Direction::SendOnly);
	int addAudio(string mid = "audio", Direction dir = Direction::SendOnly);

	std::variant<Media *, Application *> media(int index);
	std::variant<const Media *, const Application *> media(int index) const;
	size_t mediaCount() const;

	Application *application();

	static Type stringToType(const string &typeString);
	static string typeToString(Type type);

private:
	std::optional<Candidate> defaultCandidate() const;
	std::shared_ptr<Entry> createEntry(string mline, string mid, Direction dir);
	void removeApplication();

	Type mType;

	// Session-level attributes
	Role mRole;
	string mUsername;
	string mSessionId;
	std::optional<string> mIceUfrag, mIcePwd;
	std::optional<string> mFingerprint;

	// Entries
	std::vector<std::shared_ptr<Entry>> mEntries;
	std::shared_ptr<Application> mApplication;

	// Candidates
	std::vector<Candidate> mCandidates;
	bool mEnded = false;
};

} // namespace rtc

std::ostream &operator<<(std::ostream &out, const rtc::Description &description);
std::ostream &operator<<(std::ostream &out, rtc::Description::Type type);
std::ostream &operator<<(std::ostream &out, rtc::Description::Role role);

#endif
