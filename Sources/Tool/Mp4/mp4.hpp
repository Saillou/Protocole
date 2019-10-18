#pragma once

#include "commons.h"

// -- Highest level --
struct Atom {
	explicit Atom(const std::string& name_, const uint32_t size_ = 0) :
		p_name(makeName(name_)),
		p_size(8 + size_)
	{

	}

	virtual ~Atom() {
	};

	// Methods
	virtual const uint32_t name() const {
		return p_name;
	}
	virtual const uint32_t size() const {
		return p_size;
	}

	virtual puint8_t data() const {
		puint8_t p;
		p << size() << name();
		return p;
	}

	static uint32_t makeTime() {
		time_t rawtime;
		struct tm timeinfo;

		time(&rawtime);
		localtime_s(&timeinfo, &rawtime);

		return (uint32_t)mktime(&timeinfo) + 2082844800; // Convert to mac/apple timestamp
	}

	static uint32_t makeTime(int year, int month, int day, int hour, int min, int sec) {
		struct tm timeinfo = { 0 };

		timeinfo.tm_year = year - 1900;
		timeinfo.tm_mon = month - 1;    //months since January - [0,11]
		timeinfo.tm_mday = day;          //day of the month - [1,31] 
		timeinfo.tm_hour = hour;         //hours since midnight - [0,23]
		timeinfo.tm_min = min;          //minutes after the hour - [0,59]
		timeinfo.tm_sec = sec;          //seconds after the minute - [0,59]

		return (uint32_t)mktime(&timeinfo) + 2082844800; // Convert to mac/apple timestamp
	}

	static uint32_t makeName(const std::string& name_) {
		if (name_.size() != 4)
			return 0;

		uint32_t fName = 0;
		for (int i = 0; i < 4; i++)
			fName = (fName * 0x100) + (uint32_t)(uint8_t)name_[i];

		return fName;
	};
	static const std::string readName(uint32_t name_) {
		std::string name;
		while (name_ > 0) {
			name.push_back((char)(name_ % 0x100));
			name_ /= 0x100;
		}

		return std::string(name.rbegin(), name.rend());
	};

protected:
	uint32_t p_name;
	uint32_t p_size;
};

struct Version {
	explicit Version(const uint32_t version_flags_ = 0) :
		p_version(0),
		p_flags{ 0,0,0 }
	{
		if (version_flags_ != 0) {
			// Version : Byte 1
			p_version = (uint8_t)(version_flags_ >> 24);

			// Flags : Bytes 2 - 4
			uint32_t maskFlags = 0x1000000;
			uint32_t allFlags = (version_flags_ % maskFlags); // Bytes 2|3|4

			p_flags[0] = (uint8_t)(allFlags >> 16);
			p_flags[1] = (uint8_t)(allFlags >> 8) % 0x100;
			p_flags[2] = (uint8_t)(allFlags % 0x100);
		}
	}

	virtual ~Version() {
	};

	// Getters
	virtual const uint8_t version() const {
		return p_version;
	}

	virtual const uint32_t flags() const {
		uint32_t allFlags = ((uint32_t)p_flags[0] << 16) + ((uint32_t)p_flags[1] << 8) + (uint32_t)p_flags[2];
		return allFlags;
	}

	// Statics
	static const uint32_t sizeField() {
		return 4;
	}

protected:
	uint8_t p_version;
	uint8_t p_flags[3];
};

struct AtomVersion : public Atom, public Version {
	explicit AtomVersion(const std::string& name_, const uint32_t size_ = 0, const uint32_t version_flags_ = 0) :
		Atom(name_, Version::sizeField() + size_),
		Version(version_flags_)
	{

	}

	virtual ~AtomVersion() {
	};

	virtual puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << p_version << p_flags[0] << p_flags[1] << p_flags[2];
		return p;
	}
};


// -- Level 8 --
struct Avcc : public Atom {
	Avcc() : Atom("avcC", 11),
		configurationVersion(1),
		lengthSizeMinusOne(0xFC | 3), 				// 6 bits padding : 1111'11xx b | xxxx'xx[length] b <=> 0xFC | [length]. Length = 4bytes - 1.
		numOfSequenceParameterSets(0xE0 | 1),		// 3 bits padding : 111x'xxxx b | xxx[num] b <=> 0xE0 | [num]. Always 1.
		numOfPictureParameterSets(1)			 	// Always 1.
	{

	}

	// Members
	uint8_t configurationVersion;				// 0 - Always 1
	uint8_t avcProfileIndication;				// 1 - 66:baseline, 77:main profile, 88:extended profile
	uint8_t profile_compatibility;				// 2 - Constraints: (8b) b1|b2|b3|b4|0000
	uint8_t avcLevelIndication; 				// 3 - 10 * level
	uint8_t lengthSizeMinusOne; 				// 4 - 6 bits padding.

	uint8_t numOfSequenceParameterSets;	// 5 
	uint16_t sequenceParameterSetLength() const { 	// 6
		return (uint16_t)sequenceParameterSetNALUnit.size();
	}
	puint8_t sequenceParameterSetNALUnit;	// 8

	uint8_t numOfPictureParameterSets;				// 8  + length
	uint16_t pictureParameterSetLength() const {	// 9  + length
		return (uint16_t)pictureParameterSetNALUnit.size();
	}
	puint8_t pictureParameterSetNALUnit;	// 11 + length

	// Methods
	const uint32_t size() const override {
		return p_size + (uint32_t)sequenceParameterSetNALUnit.size() + (uint32_t)pictureParameterSetNALUnit.size();
	}

	// Cast
	operator puint8_t() {
		puint8_t serialized;

		// Atoms:
		serialized << size() << p_name;

		// Avcc version:
		serialized << configurationVersion
			<< avcProfileIndication
			<< profile_compatibility
			<< avcLevelIndication
			<< lengthSizeMinusOne;

		// SequenceParameterSets
		serialized << numOfSequenceParameterSets
			<< sequenceParameterSetLength()
			<< sequenceParameterSetNALUnit;

		// PictureParameterSets
		serialized << numOfPictureParameterSets
			<< pictureParameterSetLength()
			<< pictureParameterSetNALUnit;

		return serialized;
	}
};

struct Esds : public Atom {
	Esds() : Atom("esds", 36),
		version(0x0),
		esdtag(0x3),
		length_1(0x80'80'80'1B),
		priority(0x0),
		tag_1(0x4),
		length_2(0x80'80'80'0D),
		objectId(0x6B),
		streamType(0x15),
		reserved{ 0,0,0 },
		tag_2(0x6),
		length_3(0x80808001),
		tag_3(0x2)
	{

	}

	uint32_t version; 	// 0
	uint8_t esdtag;		// 4
	uint32_t length_1;	// 5
	uint16_t streamId;	// 9
	uint8_t priority;	// 11
	uint8_t tag_1;		// 12
	uint32_t length_2;	// 16
	uint8_t objectId;	// 17
	uint8_t streamType; // 18
	uint8_t reserved[3]; // 19
	uint32_t maxBitrate; // 22
	uint32_t avgBitrate; // 26
	uint8_t tag_2;		// 30
	uint32_t length_3;	// 34
	uint8_t tag_3;		// 35

	// Methods
	const uint32_t size() const override {
		return p_size;
	}

	puint8_t data() const override {
		puint8_t p;

		p << Atom::data() << version
			<< esdtag << length_1
			<< streamId
			<< priority << tag_1 << length_2
			<< objectId << streamType << puint8_t(reserved, reserved + 3)
			<< maxBitrate
			<< avgBitrate
			<< tag_2 << length_3 << tag_3;

		return p;
	}

	operator puint8_t() {
		return data();
	}
};

// -- Level 7 --
struct Avc1 {
	Avc1() :
		subVersion(0),
		revision(0),
		data_size(0),
		frame_count(1),
		comp_name(0),
		colors{ 0 },
		reserved_2(0xFFFF)
	{

	};

	uint16_t subVersion;	// 0  - 0
	uint16_t revision;		// 2 - 0
	uint32_t vendor;		// 4
	uint32_t temporal_q;	// 8 - 0|1023
	uint32_t spatial_q;		// 12 - 0|1024
	uint16_t width;			// 16
	uint16_t height;		// 18
	uint32_t reso_h;		// 20 - ( / 0x 1 00 00)
	uint32_t reso_v;		// 24
	uint32_t data_size;		// 28 - 0
	uint16_t frame_count;	// 30 - Most of the time 1
	uint32_t comp_name;		// 34 - Compressor name
	uint16_t colors[14];	// 38 - color
	uint16_t depth;			// 66
	uint16_t reserved_2;	// 68 - 0xff ff
	puint8_t config;		// 70 - video config (avcBox)

	// Methods
	const uint32_t size() const {
		return (uint32_t)70 + (uint32_t)config.size() * sizeof(uint8_t);
	}

	puint8_t data() const {
		puint8_t p;
		p << subVersion << revision
			<< vendor << temporal_q << spatial_q
			<< width << height << reso_h << reso_v
			<< data_size << frame_count << comp_name
			<< puint16_t(colors, colors + 14)
			<< depth
			<< reserved_2
			<< config;

		return p;
	}

	operator puint8_t() {
		return data();
	}
};

struct  Mp4a {
	Mp4a() :
		O1(0), O2(0), O3(0), O4(0)
	{

	}

	uint32_t O1;				// 0
	uint32_t O2;				// 4
	uint16_t channels;		// 8
	uint16_t depth;			// 10
	uint32_t O3;				// 12
	uint16_t bitrate;		// 16
	uint16_t O4; 				// 18
	puint8_t config;			// 20

	// Methods
	const uint32_t size() const {
		return (uint32_t)20 + (uint32_t)config.size() * sizeof(uint8_t);
	}

	puint8_t data() const {
		puint8_t p;

		p << O1 << O2
			<< channels << depth
			<< O3
			<< bitrate
			<< O4
			<< config;

		return p;
	}

	operator puint8_t() {
		return data();
	}
};

// -- Level 6 --
struct Ref : public AtomVersion {
	enum Flags {
		NoFlags = 0x0,
		Self = 0x1
	};

	// Constructors
	Ref(const std::string& name_, const std::string& data_ = "", uint32_t version_flags_ = Self) :
		AtomVersion(name_, 0, version_flags_)
	{
		dataRef << data_;
	};
	Ref(const std::string& name_, const puint8_t& data_, uint32_t version_flags_ = Self) :
		AtomVersion(name_, 0, version_flags_), dataRef(data_)
	{

	};

	// Members
	puint8_t dataRef;	// 0

	// Methods
	const uint32_t size() const override {
		return p_size + (uint32_t)(dataRef.size()) * sizeof(uint8_t);
	}

	puint8_t data() const override {
		puint8_t p;
		p << AtomVersion::data() << dataRef;
		return p;
	}
};

struct Tsd : public Atom {
	Tsd(const std::string& name_) : Atom(name_, 8),
		reserved_1{ 0 }
	{	};

	uint8_t reserved_1[6];	// 0
	uint16_t data_index;		// 6
	puint8_t description;	// Depends...

	// Methods
	const uint32_t size() const override {
		return p_size + (uint32_t)description.size() * sizeof(uint8_t);
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data()
			<< puint8_t(reserved_1, reserved_1 + 6)
			<< data_index
			<< description;

		return p;
	}
};

struct Tts {
	Tts(uint32_t sample_count_ = 0, uint32_t sample_duration_ = 0) :
		sample_count(sample_count_), sample_duration(sample_duration_)
	{

	};

	uint32_t sample_count;		// 0
	uint32_t sample_duration;	// 4

	static const uint32_t sizeField() {
		return 8;
	}
};

struct Tsc {
	Tsc(uint32_t first_chunk_ = 0, uint32_t samples_per_chunk_ = 0, uint32_t sample_desc_id_ = 0) :
		first_chunk(first_chunk_), samples_per_chunk(samples_per_chunk_), sample_desc_id(sample_desc_id_)
	{

	};

	uint32_t first_chunk;
	uint32_t samples_per_chunk;
	uint32_t sample_desc_id;

	static const uint32_t sizeField() {
		return 12;
	}
};


// -- Level 5 --
struct Vmhd : public AtomVersion {
	enum Flags {
		NoFlags = 0x0,
		NewMovie = 0x1,
	};

	enum ColorsMode {
		Copy = 0x0,
		Blend = 0x20,
		Transparent = 0x24,
		DitherCopy = 0x40,
		StraightAlpha = 0x100,
		PremulWhiteAlpha = 0x101,
		PremulBlackAlpha = 0x102,
		Composition = 0x103,
		StraightAplhaBlen = 0x104,
	};

	Vmhd() : AtomVersion("vmhd", 8, NewMovie), opcolor{ 0 } {};

	uint16_t graphicsmode;		// 0
	uint16_t opcolor[3];		// 2

	puint8_t data() const override {
		puint8_t p;
		p << AtomVersion::data() << graphicsmode << puint16_t(opcolor, opcolor + 3);
		return p;
	}
};
struct Smhd : public AtomVersion {
	Smhd() : AtomVersion("smhd", 4, 0x0), balance(0), reserved(0) {}

	uint16_t balance;	// 0
	uint16_t reserved;	// 2

	puint8_t data() const override {
		puint8_t p;
		p << AtomVersion::data() << balance << reserved;
		return p;
	}
};

struct Dinf : public Atom {
	Dinf() : Atom("dinf") {};

	// Members
	struct Dref : public AtomVersion {			// 0
		Dref() : AtomVersion("dref", 4) {};

		// Members
		const uint32_t entry_count() const { 		// 0
			return (uint32_t)ref.size();
		}
		std::vector<Ref> ref;							// 4

		// Methods
		const uint32_t size() const override {
			uint32_t byte_size = p_size;
			for (auto& r : ref)
				byte_size += r.size();

			return byte_size;
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << entry_count();
			for (auto& r : ref)
				p << r.data();

			return p;
		}

	} dref;

	// Methods
	const uint32_t size() const override {
		return p_size + dref.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << dref.data();
		return p;
	}

};

struct Stbl : public Atom {
	Stbl() : Atom("stbl") {};

	struct Stsd : public AtomVersion {
		Stsd() : AtomVersion("stsd", 4) {};

		// Members
		const uint32_t entry_count() const { 		// 0
			return (uint32_t)tsd.size();
		}
		std::vector<Tsd> tsd;							// 4

		// Methods
		const uint32_t size() const override {
			uint32_t byte_size = p_size;
			for (auto& t : tsd)
				byte_size += t.size();

			return byte_size;
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << entry_count();
			for (auto& t : tsd)
				p << t.data();

			return p;
		}

	} stsd;

	struct Stts : public AtomVersion {
		Stts() : AtomVersion("stts", 4) {};

		// Members
		const uint32_t entry_count() const { 		// 0
			return (uint32_t)tts.size();
		}
		std::vector<Tts> tts;							// 4 

		// Methods
		const uint32_t size() const override {
			return p_size + entry_count()*Tts::sizeField();
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << entry_count();
			for (auto& t : tts)
				p << t.sample_count << t.sample_duration;

			return p;
		}

	} stts;

	struct Stss : public AtomVersion {
		Stss() : AtomVersion("stss", 4) {};

		// Methods
		const uint32_t entry_count() const { 		// 0
			return (uint32_t)tss.size();
		}
		std::vector<uint32_t> tss;					// 4

		// Methods
		const uint32_t size() const override {
			return p_size + entry_count() * sizeof(uint32_t);
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << entry_count() << tss;

			return p;
		}

	} stss;

	struct Stsc : public AtomVersion {
		Stsc() : AtomVersion("stsc", 4) {};

		// Members
		const uint32_t entry_count() const { 		// 0
			return (uint32_t)tsc.size();
		}
		std::vector<Tsc> tsc;							// 4

		// Methods
		const uint32_t size() const override {
			return p_size + entry_count()*Tsc::sizeField();
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << entry_count();
			for (auto& t : tsc)
				p << t.first_chunk << t.samples_per_chunk << t.sample_desc_id;

			return p;
		}

	} stsc;

	struct Stsz : public AtomVersion {
		Stsz() : AtomVersion("stsz", 8), sample_size(0) {};

		// Members
		uint32_t sample_size;					// 0
		const uint32_t entry_count() const { 	// 4
			return (uint32_t)tsz.size();
		}
		std::vector<uint32_t> tsz;				// 8

		// Methods
		const uint32_t size() const override {
			return p_size + entry_count() * sizeof(uint32_t);
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << sample_size << entry_count() << tsz;

			return p;
		}

	} stsz;

	struct Stco : public AtomVersion {
		Stco() : AtomVersion("stco", 4) {};

		// Members
		const uint32_t entry_count() const { 		// 0
			return (uint32_t)tco.size();
		}
		std::vector<uint32_t> tco;					// 4

		// Methods
		const uint32_t size() const override {
			return p_size + entry_count() * sizeof(uint32_t);
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << entry_count() << tco;

			return p;
		}
	} stco;

	// Methods
	const uint32_t size() const override {
		return p_size + stsd.size() + stts.size() + stss.size() + stsc.size() + stsz.size() + stco.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << stsd.data() << stts.data() << stss.data() << stsc.data() << stsz.data() << stco.data();
		return p;
	}
};


// -- Level 4 --
struct Mdhd : public AtomVersion {
	Mdhd() : AtomVersion("mdhd", 20) {};

	uint32_t creation_time;		// 0
	uint32_t modification_time;	// 4
	uint32_t timescale;				// 8
	uint32_t duration;				// 12
	uint16_t language;				// 16
	uint16_t quality;					// 18

	puint8_t data() const override {
		puint8_t p;
		p << AtomVersion::data() << creation_time << modification_time << timescale << duration << language << quality;
		return p;
	}
};

struct Hdlr : public AtomVersion {
	Hdlr() : AtomVersion("hdlr", 20), manufacturer(0), sub_flags(0), flags_mask(0) {};

	uint32_t type;						// 0
	uint32_t subtype;					// 4
	uint32_t manufacturer;			// 8 - reserved
	uint32_t sub_flags;				// 12 - reserved
	uint32_t flags_mask;				// 16 - reserved
	puint8_t name;						// 20

	const uint32_t size() const override {
		return p_size + sizeof(uint8_t)*(uint32_t)name.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << AtomVersion::data() << type << subtype << manufacturer << sub_flags << flags_mask << name;
		return p;
	}
};

struct Minf : public Atom {
	Minf() : Atom("minf") {};

	enum MediaType {
		VIDEO,
		AUDIO
	};
	MediaType type = VIDEO;

	Vmhd vmhd;
	Smhd smhd;

	Dinf dinf;
	Stbl stbl;

	const uint32_t size() const override {
		uint32_t s = p_size;
		if (type == VIDEO)
			s += vmhd.size();
		else if (type == AUDIO)
			s += smhd.size();

		s += dinf.size() + stbl.size();
		return s;
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data();

		if (type == VIDEO)
			p << vmhd.data();
		else if (type == AUDIO)
			p << smhd.data();

		p << dinf.data() << stbl.data();
		return p;
	}
};

struct Elt {
	uint32_t track_time;			// 0
	uint32_t media_time;			// 4
	uint32_t media_rate;			// 8
};

struct Data : public Atom {
	Data(uint8_t type_) : Atom("data", 8), type(type_), local(0) {};

	// Members
	uint32_t type;
	uint32_t local;
	puint8_t value;

	// Methods
	const uint32_t size() const override {
		return p_size + sizeof(uint8_t) * (uint32_t)value.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << type << local << value;
		return p;
	}
};

struct Item : public Atom {
	Item(std::string name_, const puint8_t& value_ = {}) : Atom(name_), mdata(1) {
		if (value_.size() > 0)
			mdata.value << value_;
	}
	Item(std::string name_, const std::string& value_) : Atom(name_), mdata(1) {
		if (value_.size() > 0)
			mdata.value << value_;
	}

	// Members
	Data mdata;

	// Methods
	const uint32_t size() const override {
		return p_size + mdata.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << mdata.data();
		return p;
	}

	// Statics
	static const uint8_t At = 0xA9;
};


// -- Level 3 --
struct Tkhd : public AtomVersion {
	enum Flags {
		NoFlags = 0x0,
		TrackEnabled = 0x1,
		TrackInMovie = 0x2,
		TrackInPreview = 0x4,
		TrackInPoster = 0x8
	};

	Tkhd() : AtomVersion("tkhd", 80, TrackEnabled | TrackInMovie),
		track_ID(0),
		reserved_0(0),
		reserved_1{ 0 },
		reserved_2(0),
		matrix{
		   0x1'0000, 0, 0,
		   0, 0x1'0000, 0,
		   0, 0, 0x4000'0000
	}
	{};

	uint32_t creation_time;		// 0
	uint32_t modification_time;	// 4
	uint32_t track_ID;				// 8
	uint32_t reserved_0;			// 12
	uint32_t duration;				// 16
	uint8_t reserved_1[8];			// 20
	uint16_t layer;					// 28
	uint16_t alternate_group;		// 30
	uint16_t volume;					// 32
	uint16_t reserved_2;			// 34
	uint32_t matrix[9];				// 36
	uint32_t width;					// 72
	uint32_t height;					// 76

	puint8_t data() const override {
		puint8_t p;
		p << AtomVersion::data()
			<< creation_time << modification_time << track_ID
			<< reserved_0
			<< duration
			<< puint8_t(reserved_1, reserved_1 + 8)
			<< layer << alternate_group << volume
			<< reserved_2
			<< puint32_t(matrix, matrix + 9)
			<< width << height;

		return p;
	}
};

struct Edts : public Atom {
	Edts() : Atom("edts") {};

	struct Elst : public AtomVersion {
		Elst() : AtomVersion("elst", 4) {};

		// Members
		const uint32_t entry_count() const { 		// 0
			return (uint32_t)elt.size();
		}
		std::vector<Elt> elt;							// 4

		// Methods
		const uint32_t size() const override {
			return p_size + entry_count() * sizeof(Elt);
		}

		puint8_t data() const override {
			puint8_t p;
			p << AtomVersion::data() << entry_count();

			for (auto& e : elt)
				p << e.track_time << e.media_time << e.media_rate;

			return p;
		}
	} elst;

	const uint32_t size() const override {
		return p_size + elst.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << elst.data();
		return p;
	}
};

struct Mdia : public Atom {
	Mdia() : Atom("mdia") {};

	Mdhd mdhd;
	Hdlr hdlr;
	Minf minf;

	const uint32_t size() const override {
		return p_size + mdhd.size() + hdlr.size() + minf.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << mdhd.data() << hdlr.data() << minf.data();
		return p;
	}
};

struct Meta : public Atom {
	Meta() : Atom("meta", 4), reserved(0) {};

	uint32_t reserved;

	struct Meta_hdlr : public Atom {
		Meta_hdlr() :
			Atom("hdlr", 25),
			reserved_1{ 0 },
			mdir(makeName("mdir")),
			appl(makeName("appl")),
			reserved_2{ 0 }
		{ };

		// Members
		uint8_t reserved_1[8]; // - Reserved
		uint32_t mdir;
		uint32_t appl;
		uint8_t reserved_2[9]; 	// - Reserved

		// Methods
		puint8_t data() const override {
			puint8_t p;
			p << Atom::data() << puint8_t(reserved_1, reserved_1 + 8) << mdir << appl << puint8_t(reserved_2, reserved_2 + 9);
			return p;
		}

	} hdlr;

	struct Meta_ilst : public Atom {
		Meta_ilst() : Atom("ilst") {	}

		// Members
		std::vector<Item> items;

		// Methods
		const uint32_t size() const override {
			uint32_t byteSize = p_size;
			for (auto& i : items)
				byteSize += i.size();

			return byteSize;
		}

		puint8_t data() const override {
			puint8_t p = Atom::data();
			for (auto& i : items)
				p << i.data();

			return p;
		}

	} ilst;

	const uint32_t size() const override {
		return p_size + hdlr.size() + ilst.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << reserved << hdlr.data() << ilst.data();
		return p;
	}
};


// -- Level 2 --
struct Mvhd : public AtomVersion {
	Mvhd() : AtomVersion("mvhd", 96),
		reserved{ 0 },
		matrix{
		   0x1'0000, 0, 0,
		   0, 0x1'0000, 0,
		   0, 0, 0x4000'0000
	}
	{

	};

	uint32_t creation_time;		// 0
	uint32_t modification_time;	// 4
	uint32_t timescale;				// 8
	uint32_t duration;					// 12
	uint32_t pref_rate;				// 16
	uint16_t pref_volume;			// 20
	uint8_t reserved[10];			// 22
	uint32_t matrix[9];				// 32
	uint32_t preview_time;			// 68
	uint32_t preview_duration;	// 72
	uint32_t poster_time;			// 76
	uint32_t selection_time;		// 80
	uint32_t selection_duration;	// 84
	uint32_t current_time;			// 88
	uint32_t next_track_ID;		// 92

	puint8_t data() const override {
		puint8_t p;
		p << AtomVersion::data()
			<< creation_time << modification_time << timescale << duration
			<< pref_rate << pref_volume
			<< puint8_t(reserved, reserved + 10)
			<< puint32_t(matrix, matrix + 9)
			<< preview_time << preview_duration << poster_time
			<< selection_time << selection_duration
			<< current_time << next_track_ID;

		return p;
	}
};

struct Trak : public Atom {
	Trak() : Atom("trak") {};

	Tkhd tkhd;
	Edts edts;
	Mdia mdia;

	const uint32_t size() const override {
		return p_size + tkhd.size() + edts.size() + mdia.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << tkhd.data() << edts.data() << mdia.data();
		return p;
	}
};

struct Udta : public Atom {
	Udta() : Atom("udta") {};

	Meta meta;

	const uint32_t size() const override {
		return p_size + meta.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << meta.data();

		return p;
	}
};

struct Sample {
	enum TypeSample {
		OTHER_SAMPLE	= 0,
		SAMPLE_KEY		= (1 << 0),
		NALU_SAMPLE		= (1 << 1),
		MP3_SAMPLE		= (1 << 2),
	};

	// Constructor
	Sample(const puint8_t& buffer = {}, int type = OTHER_SAMPLE) :
		type(type)
	{
		if (!buffer.empty())
			packets << buffer;
	}

	static Sample fromRawNalu(const puint8_t& buffer) {
		// Trunc mark
		static const puint8_t NALU = { 0x0, 0x0, 0x0, 0x1 };

		Sample s;
		s.type = NALU_SAMPLE;
		size_t indiceNalu = 0;
		size_t indiceBeg = 0;

		for (size_t i = 0; i < buffer.size(); i++) {
			uint8_t c = buffer[i];

			// Nalu search
			if (c == NALU[indiceNalu])
				indiceNalu++;
			else
				indiceNalu = 0;

			// Add packet
			if (indiceNalu == NALU.size()) {
				if (i - indiceBeg > indiceNalu) // not empty interval
					s << puint8_t(buffer.begin() + indiceBeg, buffer.begin() + (i - indiceNalu + 1));

				indiceNalu = 0;
				indiceBeg = i + 1; // Next byte will be interesting
			}
		}

		// Add packet
		if (indiceBeg < buffer.size()) // not empty interval
			s << puint8_t(buffer.begin() + indiceBeg, buffer.end());

		// Packets.Size <=> Keyframe? : if 1 -> normal frame, else if > 3 bunch of frames, but if 3 => (most probably) sps | pps | frame (== key frame).
		if (s.packets.size() == 3)
			s.type |= SAMPLE_KEY;

		return s;
	}

	static Sample fromRawMp3(const puint8_t& buffer) {
		// Trunc mark
		static const puint8_t MP3 = { 0xFF, 0xFB };

		Sample s;
		s.type = MP3_SAMPLE;
		size_t indice = 0;
		size_t indiceBeg = 0;

		for (size_t i = 0; i < buffer.size(); i++) {
			uint8_t c = buffer[i];

			// MP3 header search
			if (c == MP3[indice])
				indice++;
			else
				indice = 0;

			// Add packet
			if (indice == MP3.size()) {
				if (i - indiceBeg > indice) {// not empty interval
					s << puint8_t(buffer.begin() + indiceBeg, buffer.begin() + (i - indice + 1));
				}

				indice = 0;
				indiceBeg = i + 1; // Next byte will be interesting
			}
		}

		// Add packet
		if (indiceBeg < buffer.size()) // not empty interval
			s << puint8_t(buffer.begin() + indiceBeg, buffer.end());

		return s;
	}

	const uint32_t size() const {
		uint32_t byteSize = 0;

		for (auto& p : packets)
			byteSize += (uint32_t)p.size();

		return byteSize;
	}

	puint8_t data() const {
		puint8_t p;

		for (auto& packet : packets)
			p << packet;

		return p;
	}

	// Members
	int type = OTHER_SAMPLE;
	std::vector<puint8_t> packets;

	// Operators
	Sample& operator<<(const puint8_t& dts) {
		puint8_t packet;

		// Header depend on type
		switch (type) {
		case NALU_SAMPLE:
			packet << (uint32_t)(dts.size());
			break;

		case MP3_SAMPLE:
			packet << puint8_t{ 0xFF, 0xFB };
			break;
		}

		// Packet content
		packet << dts;

		// Finally
		packets << packet;
		return *this;
	};

	const puint8_t& operator[](const size_t i) const {
		return packets[i];
	}
};


// -- Level 1 --
struct Ftyp : public Atom {
	enum Version {
		Version_1_0 = 0x100,
		Version_2_0 = 0x200,
	};

	Ftyp() : Atom("ftyp", 8) {};

	uint32_t major_brand;			// 0
	uint32_t minor_version;		// 4
	puint32_t compatible_brands;	// 8

	const uint32_t size() const {
		return p_size + sizeof(uint32_t)*(uint32_t)compatible_brands.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << major_brand << minor_version << compatible_brands;
		return p;
	}
};

struct Free : public Atom {
	Free() : Atom("free") {};

	puint8_t fdat;

	const uint32_t size() const {
		return p_size + sizeof(uint32_t)*(uint32_t)fdat.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data() << fdat;
		return p;
	}
};

struct Mdat : public Atom {
	enum Mode {
		Copy,
		Placebo
	};

	Mdat() : Atom("mdat"), data_size(0), mode(Placebo) {};

	// Members
	Mode mode;
	uint32_t data_size;

	std::vector<Sample> samples; 			// copy
	std::vector<uint32_t> samples_size; // placebo

	const uint32_t size() const override {
		return p_size + data_size;
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data();

		if (mode != Copy) // No real data are stored
			return p;

		for (auto& sample : samples)
			p << sample.data();

		return p;
	}

	Mdat& operator<<(const Sample& sample) {
		if (mode == Copy)
			samples << sample;

		if (mode == Placebo)
			samples_size << (uint32_t)sample.size();

		data_size += sample.size();

		return *this;
	}
};

struct Moov : public Atom {
	Moov() : Atom("moov") {};

	Mvhd mvhd;
	std::vector<Trak> traks;
	Udta udta;

	const uint32_t size() const override {
		uint32_t trakSize = 0;
		for (auto& trak : traks)
			trakSize += trak.size();

		return p_size + mvhd.size() + trakSize + udta.size();
	}

	puint8_t data() const override {
		puint8_t p;
		p << Atom::data();
		p << mvhd.data();

		for (auto& trak : traks)
			p << trak.data();

		p << udta.data();
		return p;
	}

	Moov& operator<<(const Trak& trak) {
		traks.push_back(trak);
		return *this;
	}
};


// -- Level 0 --
struct Mp4 {
	Ftyp ftyp;
	Free free;
	Mdat mdat;
	Moov moov;

	const uint32_t size() const {
		return ftyp.size() + free.size() + mdat.size() + moov.size();
	}

	puint8_t data() const {
		puint8_t p;
		p << ftyp.data() << free.data() << mdat.data() << moov.data();
		return p;
	}
};