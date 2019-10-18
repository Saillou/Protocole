#pragma once

#include <fstream>

#include "commons.h"
#include "mp4.hpp"
#include "avcc.hpp"

class mp4Creator {
public:
	// Types
	enum SampleType {
		Video = (1 << 0),
		Audio = (1 << 1)
	};

	// Nested sub-objects
	class Chunk {
	public:
		struct ChunkElement {
			Sample sample;
			uint64_t timestamp;
		};

	public:
		// Constructors
		explicit Chunk(SampleType sampleType) : 
			sampleType(sampleType) 
		{	}

		Chunk (SampleType sampleType, const ChunkElement& elt) :
			sampleType(sampleType),
			elements(1, elt)
		{ }

		Chunk (SampleType sampleType, const std::vector<ChunkElement>& elts) :
			sampleType(sampleType),
			elements(elts)
		{ }

		// Methods
		bool addData(const Sample& data, uint64_t timestamp = 0) {
			if (data.packets.empty())
				return false;

			elements.push_back({ data, timestamp });
			return true;
		}

		// Members
		SampleType sampleType;
		std::vector<ChunkElement> elements;
	};

public:
	// Constructors
	mp4Creator() :
		_sampleRate(44100),
		_numChannels(1),
		_audioRate(44100/1024.0),
		_videoRate(30.0)
	{

	}

	// Methods
	bool open(std::string path, int numChannels, int sampleRate, int bufferSize, int fps) {
		// Reset
		_mp4 = Mp4();
		_videoTrack = Trak();
		_audioTrack = Trak();
		
		// Set params
		_sampleRate		= sampleRate;
		_numChannels	= numChannels;
		_audioRate		= (double)(sampleRate) / (bufferSize / numChannels);
		_videoRate		= fps;

		// Create file
		_file.open(path, std::ios::binary | std::ios::trunc);

		// Write header
		_mp4.ftyp.major_brand		= Atom::makeName("isom");
		_mp4.ftyp.minor_version		= Ftyp::Version_2_0;
		_mp4.ftyp.compatible_brands << Atom::makeName("isom") << Atom::makeName("iso2") << Atom::makeName("avc1") << Atom::makeName("mp41");

		_file.write((char*)_mp4.ftyp.data().data(), _mp4.ftyp.size());
		_file.write((char*)_mp4.free.data().data(), _mp4.free.size());
		_file.write((char*)_mp4.mdat.data().data(), _mp4.mdat.size()); // <---- size will be wrong

		return _file.is_open();
	}

	bool close() {
		if (!_file.is_open())
			return true;

		// Set missing infomation
		uint32_t audioDuration = _computeTrakDuration(_audioTrack, _audioRate);
		uint32_t videoDuration = _computeTrakDuration(_videoTrack, _videoRate);

		_setTrakDuration(_audioTrack, audioDuration);
		_setTrakDuration(_videoTrack, videoDuration);

		// Max?
		uint32_t duration = videoDuration > audioDuration ? videoDuration : audioDuration;

		// Write movie
		// _mp4.moov.mvhd : movie header
		_mp4.moov.mvhd.creation_time		= Atom::makeTime();
		_mp4.moov.mvhd.modification_time	= _mp4.moov.mvhd.creation_time;
		_mp4.moov.mvhd.timescale			= _MOVIE_TIME_SCALE;
		_mp4.moov.mvhd.duration				= duration;
		_mp4.moov.mvhd.pref_rate			= 0x1'0000; // value / 0x 1 00 00
		_mp4.moov.mvhd.pref_volume			= 0x100;	// value / 0x 1 00 (%)
		_mp4.moov.mvhd.preview_time			= 0;
		_mp4.moov.mvhd.preview_duration		= 0;
		_mp4.moov.mvhd.poster_time			= 0;
		_mp4.moov.mvhd.selection_time		= 0;
		_mp4.moov.mvhd.selection_duration	= 0;
		_mp4.moov.mvhd.current_time			= 0;
		_mp4.moov.mvhd.next_track_ID		= 3;

		// Write tracks
		// _mp4.moov.traks : container for all traks (trak = an individual track or stream)	
		if(audioDuration > 0) // Not empty
			_mp4.moov.traks << _audioTrack;

		if (videoDuration > 0) // Not empty
			_mp4.moov.traks << _videoTrack;

		// Write metadata
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("nam"), "Gaze Tracking Video");
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("ART"), "User");
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("mak"), "Gazo.Ltd");
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("cpy"), "Gazo.Ltd.2019");
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("too"), "Gazo.Software-v.1.2");
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("alb"), "Gaze Tracking");
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("gen"), "Analysis");
		_mp4.moov.udta.meta.ilst.items << Item(Item::At + std::string("cmt"), "Video made for analysising the gaze position.");

		_file.write((char*)_mp4.moov.data().data(), _mp4.moov.size());

		// Correct data size
		puint8_t sizeData;
		sizeData << _mp4.mdat.size();

		_file.seekp(_mp4.ftyp.size() + _mp4.free.size(), _file.beg);
		_file.write((char*)sizeData.data(), sizeData.size());

		_file.close();
		return !_file.is_open();
	}

	bool addChunk(const Chunk& chunk) {
		if (chunk.elements.empty())
			return false;

		// Choose variables
		if (chunk.sampleType == Audio) {
			// Has been init ?
			if (_audioTrack.tkhd.track_ID == 0) {
				_audioTrack = _initAudioTrack(_sampleRate, _numChannels);
			}

			if (_audioTrack.tkhd.track_ID > 0) // inited
				return _addChunk(_audioTrack, chunk, _audioRate);
		}

		else if (chunk.sampleType == Video) {
			// Has been init ?
			if (_videoTrack.tkhd.track_ID == 0) {
				// need h264 headers (sps, pps)
				if (chunk.elements.front().sample.type & Sample::SAMPLE_KEY) {
					const Sample& keySample = chunk.elements.front().sample;

					puint8_t sps(keySample[0].begin() + 4, keySample[0].end()); // remove packet header 
					puint8_t pps(keySample[1].begin() + 4, keySample[1].end()); // remove packet header 

					_videoTrack = _initVideoTrack(sps, pps);
				}				
			}

			if (_videoTrack.tkhd.track_ID > 0) // inited
				return _addChunk(_videoTrack, chunk, _videoRate);
		}

		return false; // no chunk added
	}

private:
	// Methods
	Trak _initVideoTrack(puint8_t sps, puint8_t pps) {
		Trak vTrack;
		
		// Parse info
		AvccParser avccParsed(sps, pps);

		// vTrack.tkhd : track header
		vTrack.tkhd.creation_time		= Atom::makeTime();
		vTrack.tkhd.modification_time	= vTrack.tkhd.creation_time;
		vTrack.tkhd.track_ID			= 2;
		vTrack.tkhd.duration			= 0; // <------------------------------------ Will be changed
		vTrack.tkhd.layer				= 0;
		vTrack.tkhd.alternate_group		= 0;
		vTrack.tkhd.volume				= 0x100;
		vTrack.tkhd.width				= avccParsed.sps.width  * 0x1'0000; // value / 0x 1 00 00
		vTrack.tkhd.height				= avccParsed.sps.height * 0x1'0000;

		// vTrack.edts : edit list container;
		Elt elt;
		elt.track_time = 0; // <------------------------------------ Will be changed
		elt.media_time = 0;
		elt.media_rate = 0x1'0000; // value / 0x 1 00 00
		vTrack.edts.elst.elt << elt;

		// vTrack.mdia : container for the media information in a track;
		// vTrack.mdia.mdhd;
		vTrack.mdia.mdhd.creation_time		= vTrack.tkhd.creation_time;
		vTrack.mdia.mdhd.modification_time	= vTrack.mdia.mdhd.creation_time;
		vTrack.mdia.mdhd.timescale			= _MOVIE_TIME_SCALE;
		vTrack.mdia.mdhd.duration			= 0;	// <------------------------------------ Will be changed
		vTrack.mdia.mdhd.language			= 0x55C4; // Undefined
		vTrack.mdia.mdhd.quality			= 0;

		// vTrack.mdia.hdlr;
		vTrack.mdia.hdlr.type		= Atom::makeName("mhlr");
		vTrack.mdia.hdlr.subtype	= Atom::makeName("vide");
		vTrack.mdia.hdlr.name << "VideoHandler" << '\0';

		// vTrack.mdia.minf;
		vTrack.mdia.minf.type				= Minf::VIDEO;
		vTrack.mdia.minf.vmhd.graphicsmode	= Vmhd::Copy;

		vTrack.mdia.minf.dinf.dref.ref << Ref("url ", "");

		// vTrack.mdia.minf.stbl;
		// Compressor
		Avc1 avc1;
		avc1.vendor		= 0;
		avc1.temporal_q = 0;
		avc1.spatial_q	= 0;
		avc1.width	= avccParsed.sps.width;
		avc1.height = avccParsed.sps.height;
		avc1.reso_h = 72 * 0x1'0000; // value / 0x 1 00 00
		avc1.reso_v = 72 * 0x1'0000; // value / 0x 1 00 00
		avc1.depth	= 24;

		Avcc avcConfig;
		avcConfig.avcProfileIndication	= avccParsed.sps.profile_idc;
		avcConfig.profile_compatibility = avccParsed.sps.constraints;
		avcConfig.avcLevelIndication	= avccParsed.sps.level_idc;

		avcConfig.sequenceParameterSetNALUnit << avccParsed.sps.raw;
		avcConfig.pictureParameterSetNALUnit << avccParsed.pps.raw;

		avc1.config = avcConfig;

		Tsd tsd("avc1");
		tsd.data_index	= 1;
		tsd.description = avc1;
		vTrack.mdia.minf.stbl.stsd.tsd << tsd;

		return vTrack;
	}
	Trak _initAudioTrack(uint32_t bitrate, uint32_t numChannels) const {
		Trak aTrack;

		// mp4.moov.trak.tkhd : track header
		aTrack.tkhd.creation_time		= Atom::makeTime();
		aTrack.tkhd.modification_time	= aTrack.tkhd.creation_time;
		aTrack.tkhd.track_ID			= 1;
		aTrack.tkhd.duration			= 0; // <------------------------------------ Will be changed
		aTrack.tkhd.layer				= 1;
		aTrack.tkhd.alternate_group		= 0;
		aTrack.tkhd.volume				= 0x100;
		aTrack.tkhd.width				= 0;
		aTrack.tkhd.height				= 0;

		// aTrack.edts : edit list container;
		Elt elt;
		elt.track_time = 0; // <------------------------------------ Will be changed
		elt.media_time = 0;
		elt.media_rate = 0x1'0000; // value / 0x 1 00 00
		aTrack.edts.elst.elt << elt;


		// aTrack.mdia : container for the media information in a track;
		// aTrack.mdia.mdhd;
		aTrack.mdia.mdhd.creation_time		= aTrack.tkhd.creation_time;
		aTrack.mdia.mdhd.modification_time	= aTrack.mdia.mdhd.creation_time;
		aTrack.mdia.mdhd.timescale			= _MOVIE_TIME_SCALE;
		aTrack.mdia.mdhd.duration			= 0; // <------------------------------------ Will be changed
		aTrack.mdia.mdhd.language			= 0x55C4; // Undefined
		aTrack.mdia.mdhd.quality			= 0;

		// aTrack.mdia.hdlr;
		aTrack.mdia.hdlr.type		= Atom::makeName("mhlr");
		aTrack.mdia.hdlr.subtype	= Atom::makeName("soun");
		aTrack.mdia.hdlr.name << "SoundHandler" << '\0';

		// aTrack.mdia.minf;
		aTrack.mdia.minf.type = Minf::AUDIO;
		aTrack.mdia.minf.dinf.dref.ref << Ref("url ", "");

		// Description sample
		Mp4a mp4a;
		mp4a.channels	= numChannels;
		mp4a.depth		= 8;
		mp4a.bitrate	= bitrate;

		Esds esds;
		esds.streamId	= 0x1;
		esds.maxBitrate = 64'000;
		esds.avgBitrate = bitrate;
		mp4a.config		= esds;

		Tsd tsd("mp4a");
		tsd.data_index	= 1;
		tsd.description = mp4a;

		aTrack.mdia.minf.stbl.stsd.tsd << tsd;

		return aTrack;
	}

	bool _addChunk(Trak& trak, const Chunk& chunk, double rate) {
		// -- Chunk --
		// Description
		Tsc tsc;
		tsc.first_chunk			= (uint32_t)(trak.mdia.minf.stbl.stsc.entry_count()+1);
		tsc.samples_per_chunk	= (uint32_t)chunk.elements.size();
		tsc.sample_desc_id		= (uint32_t)1;

		// Offset
		uint32_t tco = (uint32_t)_file.tellp();
		
		// -- Samples --
		// Write data | Samples Sizes / Samples Key
		puint32_t tsz, tss;
		for (auto& chunkElt : chunk.elements) {
			// Write
			_mp4.mdat << chunkElt.sample;
			_file.write((char*)chunkElt.sample.data().data(), chunkElt.sample.size());

			// Size
			tsz << chunkElt.sample.size();

			// Key
			if (chunkElt.sample.type & Sample::SAMPLE_KEY)
				tss << (uint32_t)(trak.mdia.minf.stbl.stsz.entry_count() + tsz.size()); // Compute id of this sample in this trak
		}		

		// Samples timing
		Tts tts;
		tts.sample_count	= (uint32_t)chunk.elements.size();
		tts.sample_duration = (uint32_t)(trak.mdia.mdhd.timescale / rate);


		// Add infos
		trak.mdia.minf.stbl.stts.tts << tts; // Sample timing
		trak.mdia.minf.stbl.stsz.tsz << tsz; // Samples sizes
		trak.mdia.minf.stbl.stss.tss << tss; // Samples count

		trak.mdia.minf.stbl.stsc.tsc << tsc; // Chunk description
		trak.mdia.minf.stbl.stco.tco << tco; // Chunk offset

		return true;
	}

	void _setTrakDuration(Trak& trak, const uint32_t duration) {
		trak.tkhd.duration		= duration;
		trak.mdia.mdhd.duration	= duration;

		if (!trak.edts.elst.elt.empty()) {
			trak.edts.elst.elt[0].track_time = duration;
			trak.edts.elst.elt[0].media_time = 0;
		}
	}

	uint32_t _computeTrakDuration(const Trak& trak, const double rate) const {
		return (uint32_t)(trak.mdia.minf.stbl.stsz.entry_count() * _MOVIE_TIME_SCALE / rate);
	}

	// Members
	std::ofstream _file;

	Mp4 _mp4;
	Trak _videoTrack;
	Trak _audioTrack;

	uint32_t _sampleRate;
	uint32_t _numChannels;

	double _audioRate; // ~ buffer rate (sample rate/buffer size per channel)
	double _videoRate; // ~ fps

	// Constantes
	const uint32_t _MOVIE_TIME_SCALE = 1000; // 1 unit <=> 1 ms
};