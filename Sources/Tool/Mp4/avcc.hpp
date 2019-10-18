#pragma once

#include "commons.h"

// Functions
static void display(const std::string& name, const puint8_t& array) {
	std::cout << name << ": ";
	for(auto p : array) std::cout << "0x" << (int)p << " ";		
	std::cout << "\n";	
}

static std::vector<bool> toBin(uint8_t nbDec) {
	std::vector<bool> nbBin;
	while(nbDec > 0) {
		nbBin.push_back(nbDec % 2);
		nbDec /= 2;
	}
	while(nbBin.size() < 8)
		nbBin.push_back(0);
	
	return std::vector<bool>(nbBin.rbegin(), nbBin.rend());
}

static uint32_t toDec(const std::vector<bool>& nbBin, size_t iBeg, size_t nbBits) {
	if(nbBits > 32)
		return -1;
	
	uint32_t nb = 0;
	for(size_t i = iBeg; i < iBeg + nbBits && i < nbBin.size(); i++) {
		nb *= 2;
		if(nbBin[i]) 
			nb++;
	}
	
	return nb;
}

// Modify index
static uint32_t readBits(const std::vector<bool>& nbBin, size_t* pIndex, size_t nbBits) {
	if(!pIndex || nbBits < 1)
		return 0;
		
	uint32_t result = toDec(nbBin, *pIndex, nbBits);
	*pIndex += nbBits;
	
	return result;
}

static uint32_t readUe(const std::vector<bool>& nbBin, size_t* pIndex) {
	if(!pIndex)
		return 0;
	
	// Exp-Golomb:	2n+1 bits => n-bits (==0) | 1 | n-bits.
	// Formulae:		(x+1) = 1| n-bits.
	size_t& index = *pIndex;
	size_t n = 0;
	
	while(index < nbBin.size() && !nbBin[index]) {
		index++;
		n++;
	}
	
	// Apply formulae
	uint32_t result = toDec(nbBin, index, n+1)-1;
	
	// Update index
	index += (n+1);
	return result;
}
static int32_t readSe(const std::vector<bool>& nbBin, size_t* pIndex) {
	if(!pIndex)
		return 0;

   int32_t result = (int32_t)readUe(nbBin, pIndex);
	
    if (result & 0x01)
        result = (result+1)/2;
    else
        result = -(result/2);
	
    return result;
}



// Level 1
struct SPS {
	// Constructor
	SPS(const puint8_t& raw_, bool volubile_) : raw(raw_), flagVolubile(volubile_) {
		// Need binary format
		for(auto p : raw) bin << toBin(p);
		size_t index = 0; // Index of bin
		
		// Begin
		index = 1; // Reserved : 0
		uint32_t nal_ref_idc 	= readBits(bin, &index, 2);
		uint32_t nal_unit_type	= readBits(bin, &index, 5);
		
		// Profil
		profile_idc = (uint8_t)readBits(bin, &index, 8);

		// Constraints
		constraints = (uint8_t)readBits(bin, &index, 8);
		
		// Level
		level_idc 	= (uint8_t)readBits(bin, &index, 8);
		
		// seq_parameter_set_id
		uint32_t seq_parameter_set_id = readUe(bin, &index);
		
		// Weird things
		uint32_t log2_max_frame_num_minus4 	= readUe(bin, &index);
		uint32_t pic_order_cnt_type 				= readUe(bin, &index);
		
		// If pic_order_cnt_type is 0:
		uint32_t log2_max_pic_order_cnt_lsb_minus4 = readUe(bin, &index);
		
		// else .. other things.., normally no.
		
		// About frame
		uint32_t max_num_ref_frames 							= readUe(bin, &index);
		uint32_t gaps_in_frame_num_value_allowed_flag	= readBits(bin, &index, 1);
		uint32_t pic_width_in_mbs_minus1 					= readUe(bin, &index);
		uint32_t pic_height_in_map_units_minus1 			= readUe(bin, &index);
		uint32_t frame_mbs_only_flag 						= readBits(bin, &index, 1);
		
		if(!frame_mbs_only_flag) // other info, don't care (normally no)
			readUe(bin, &index);
		
		// Cropping
		uint32_t direct_8x8_inference_flag = readBits(bin, &index, 1);
		uint32_t frame_cropping_flag = readBits(bin, &index, 1);
		
		uint32_t frame_crop_left_offset 	= 0;
		uint32_t frame_crop_right_offset 	= 0;
		uint32_t frame_crop_top_offset 		= 0;
		uint32_t frame_crop_bottom_offset 	= 0;
		
		if(frame_cropping_flag != 0)	{
			frame_crop_left_offset 	= readUe(bin, &index);
			frame_crop_right_offset 	= readUe(bin, &index);
			frame_crop_top_offset 		= readUe(bin, &index);
			frame_crop_bottom_offset 	= readUe(bin, &index);
		}
		
		width 	= (uint16_t)((pic_width_in_mbs_minus1 +1)*16) - frame_crop_right_offset *2 - frame_crop_left_offset *2;
		height 	= (uint16_t)((2 - frame_mbs_only_flag)* (pic_height_in_map_units_minus1 +1) * 16) - (frame_crop_bottom_offset* 2) - (frame_crop_top_offset* 2);	
		
		uint32_t vui_parameters_present_flag = readBits(bin, &index, 1);
		if(!vui_parameters_present_flag)
			return; // ended - normally no
		
		uint32_t aspect_ratio_info_present_flag 		= readBits(bin, &index, 1);
		uint32_t overscan_info_present_flag  			= readBits(bin, &index, 1);
		uint32_t video_signal_type_present_flag 		= readBits(bin, &index, 1);
		uint32_t chroma_loc_info_present_flag 		= readBits(bin, &index, 1);
		uint32_t timing_info_present_flag 				= readBits(bin, &index, 1);
		uint32_t nal_hrd_parameters_present_flag	= readBits(bin, &index, 1);
		uint32_t vcl_hrd_parameters_present_flag	= readBits(bin, &index, 1);
		uint32_t pic_struct_present_flag				= readBits(bin, &index, 1);
		uint32_t bitstream_restriction_flag 			= readBits(bin, &index, 1);
		
		uint32_t motion_vectors_over_pic_boundaries_flag = 0;
		uint32_t max_bytes_per_pic_denom = 0;
		uint32_t max_bits_per_mb_denom = 0;
		uint32_t log2_max_mv_length_horizontal = 0;
		uint32_t log2_max_mv_length_vertical = 0;
		uint32_t num_reorder_frames = 0;
		uint32_t max_dec_frame_buffering = 0;
		
		if(bitstream_restriction_flag) {
			motion_vectors_over_pic_boundaries_flag	= readBits(bin, &index, 1);
			max_bytes_per_pic_denom 						= readUe(bin, &index);
			max_bits_per_mb_denom 							= readUe(bin, &index);
			log2_max_mv_length_horizontal 				= readUe(bin, &index);
			log2_max_mv_length_vertical 					= readUe(bin, &index);
			num_reorder_frames 								= readUe(bin, &index);
			max_dec_frame_buffering 						= readUe(bin, &index);
		}
		
		// End (after that, other bit are 0 until the end of the byte)
		if(!flagVolubile)
			return;
		
		// Results
		std::cout << std::dec;
		std::cout << " ------------ Show SPS ------------ "<< std::endl << std::endl;
		
		std::cout << "nal_ref_idc  : " 	<< nal_ref_idc								<< std::endl;
		std::cout << "nalu_type    : " 	<< nal_unit_type							<< std::endl;
		std::cout << "Profil       : " 	<< (int)profile_idc						<< std::endl;
		std::cout << "Constraints  : "	<< (int)((constraints & 128) != 0)	<< (int)((constraints & 64) != 0)	<< (int)((constraints & 32) != 0) << (int)((constraints & 16) != 0)	<< std::endl;
		std::cout << "Level        : " 	<< (int)level_idc 						<< std::endl;
		std::cout << "Param set id : "	<< seq_parameter_set_id 		<< std::endl << std::endl;
		
		std::cout << "Max frame num                        : "	<< log2_max_frame_num_minus4 			<< std::endl;
		std::cout << "Order cnt type                       : "	<< pic_order_cnt_type 						<< std::endl;
		std::cout << " -> Order cnt max: "	<< log2_max_pic_order_cnt_lsb_minus4 	<< std::endl;		
		std::cout << "max_num_ref_frames                   : "	<< max_num_ref_frames 							<< std::endl;
		std::cout << "gaps_in_frame_num_value_allowed_flag : "	<< gaps_in_frame_num_value_allowed_flag 	<< std::endl;
		std::cout << "pic_width_in_mbs_minus1              : "	<< pic_width_in_mbs_minus1 					<< std::endl;
		std::cout << "pic_height_in_map_units_minus1       : "	<< pic_height_in_map_units_minus1 			<< std::endl;
		std::cout << "frame_mbs_only_flag                  : "	<< frame_mbs_only_flag 							<< std::endl << std::endl;
		
		std::cout << "direct_8x8_inference_flag : "		<< direct_8x8_inference_flag 	<< std::endl;
		std::cout << "frame_cropping_flag       : "		<< frame_cropping_flag 				<< std::endl;
		std::cout << " -> frame_crop_left_offset  : "	<< frame_crop_left_offset 		<< std::endl;
		std::cout << " -> frame_crop_right_offset : "	<< frame_crop_right_offset 		<< std::endl;
		std::cout << " -> frame_crop_top_offset   : "	<< frame_crop_top_offset 			<< std::endl;
		std::cout << " -> frame_crop_bottom_offset: "	<< frame_crop_bottom_offset 		<< std::endl << std::endl;
		
		std::cout << "vui_parameters_present_flag: "	<< vui_parameters_present_flag 		<< std::endl;
		std::cout << " -> aspect_ratio_info_present_flag  : "	<< aspect_ratio_info_present_flag		<< std::endl;
		std::cout << " -> overscan_info_present_flag      : " 	<< overscan_info_present_flag 			<< std::endl;
		std::cout << " -> video_signal_type_present_flag  : " 	<< video_signal_type_present_flag     	<< std::endl;
		std::cout << " -> chroma_loc_info_present_flag    : " 	<< chroma_loc_info_present_flag     	<< std::endl;
		std::cout << " -> timing_info_present_flag        : " 	<< timing_info_present_flag         		<< std::endl;
		std::cout << " -> nal_hrd_parameters_present_flag : " 	<< nal_hrd_parameters_present_flag 	<< std::endl;
		std::cout << " -> vcl_hrd_parameters_present_flag : "	<< vcl_hrd_parameters_present_flag     << std::endl;
		std::cout << " -> pic_struct_present_flag         : " 	<< pic_struct_present_flag    			<< std::endl;
		std::cout << " -> bitstream_restriction_flag      : " 	<< bitstream_restriction_flag         	<< std::endl;
		std::cout << " ---> motion_vectors_over_pic_boundaries_flag : "	<< motion_vectors_over_pic_boundaries_flag 		<< std::endl;
		std::cout << " ---> max_bytes_per_pic_denom                 : "	<< max_bytes_per_pic_denom 							<< std::endl;
		std::cout << " ---> max_bits_per_mb_denom                   : "	<< max_bits_per_mb_denom 								<< std::endl;
		std::cout << " ---> log2_max_mv_length_horizontal           : "	<< log2_max_mv_length_horizontal 					<< std::endl;
		std::cout << " ---> log2_max_mv_length_vertical             : "	<< log2_max_mv_length_vertical 						<< std::endl;
		std::cout << " ---> num_reorder_frames                      : "	<< num_reorder_frames 									<< std::endl;
		std::cout << " ---> max_dec_frame_buffering                 : "	<< max_dec_frame_buffering 							<< std::endl << std::endl;
		
		std::cout << index << "/" << bin.size() << "\n";
					
		std::cout << std::hex << std::endl;
	}
	
	// Members
	const puint8_t& raw;
	std::vector<bool> bin;
	
	uint8_t profile_idc;
	uint8_t constraints;
	uint8_t level_idc;
	
	uint16_t width;
	uint16_t height;
	
	bool flagVolubile;
};

struct PPS {
	// Constructor
	PPS(const puint8_t& raw_, bool volubile_ = false) : raw(raw_), flagVolubile(volubile_) {
		// Need binary format
		for(auto p : raw) bin << toBin(p);
		size_t index = 0; // Index of bin
	
		uint32_t pic_parameter_set_id  		= readUe(bin, &index);
		uint32_t seq_parameter_set_id  		= readUe(bin, &index);
		uint32_t entropy_coding_mode_flag 	= readBits(bin, &index, 1);
		uint32_t pic_order_present_flag 	= readBits(bin, &index, 1);
		uint32_t num_slice_groups_minus1 	= readUe(bin, &index);
		
		uint32_t num_ref_idx_l0_active_minus1    					= readUe(bin, &index);
		uint32_t num_ref_idx_l1_active_minus1    					= readUe(bin, &index);
		uint32_t weighted_pred_flag    								= readBits(bin, &index, 1);
		uint32_t weighted_bipred_idc    								= readBits(bin, &index, 2);
		uint32_t pic_init_qp_minus26    								= readSe(bin, &index);
		uint32_t pic_init_qs_minus26    								= readSe(bin, &index);
		uint32_t chroma_qp_index_offset    							= readSe(bin, &index);
		uint32_t deblocking_filter_control_present_flag    	= readBits(bin, &index, 1);
		uint32_t constrained_intra_pred_flag    					= readBits(bin, &index, 1);
		uint32_t redundant_pic_cnt_present_flag        			= readBits(bin, &index, 1);
		
		// ..
		// End (after that, other bit are 0 until the end of the byte)	
		if(!flagVolubile)
			return;
		
		
		// Results
		std::cout << std::dec;
		std::cout << " ------------ Show PPS ------------ "<< std::endl << std::endl;
		
		std::cout << "pic_parameter_set_id : " 	<< pic_parameter_set_id	<< std::endl;
		std::cout << "seq_parameter_set_id : " 	<< seq_parameter_set_id	<< std::endl;
		std::cout << "entropy_coding_mode_flag  : " 	<< entropy_coding_mode_flag	<< std::endl;
		std::cout << "pic_order_present_flag    : " 	<< pic_order_present_flag    	<< std::endl;
		std::cout << "num_slice_groups_minus1   : " 	<< num_slice_groups_minus1   	<< std::endl << std::endl;
		
		std::cout << "num_ref_idx_l0_active_minus1           : " << num_ref_idx_l0_active_minus1 					<< std::endl;
		std::cout << "num_ref_idx_l1_active_minus1           : " << num_ref_idx_l1_active_minus1 					<< std::endl;
		std::cout << "weighted_pred_flag                     : " << weighted_pred_flag 								<< std::endl;
		std::cout << "weighted_bipred_idc                    : " << weighted_bipred_idc 								<< std::endl;
		std::cout << "pic_init_qp_minus26                    : " << pic_init_qp_minus26 								<< std::endl;
		std::cout << "pic_init_qs_minus26                    : " << pic_init_qs_minus26 								<< std::endl;
		std::cout << "chroma_qp_index_offset                 : " << chroma_qp_index_offset 							<< std::endl;
		std::cout << "deblocking_filter_control_present_flag : " << deblocking_filter_control_present_flag	<< std::endl;
		std::cout << "constrained_intra_pred_flag            : " << constrained_intra_pred_flag 					<< std::endl;
		std::cout << "redundant_pic_cnt_present_flag         : " << redundant_pic_cnt_present_flag 				<< std::endl << std::endl;
		
		std::cout << index << "/" << bin.size() << "\n";
		
		std::cout << std::hex << std::endl;
	}	
	
	// Members
	const puint8_t& raw;
	std::vector<bool> bin;
	
	bool flagVolubile;
};


// Main
struct AvccParser {
	// Constructor
	AvccParser(const puint8_t& sps_, const puint8_t& pps_, bool volubile = false) : 
		sps(sps_, volubile), 
		pps(pps_, volubile) 
	{

	};
		
	// Members
	SPS sps;
	PPS pps;
};