/**
 * @file
 * @brief Source file for Tracker effect class
 * @author Jonathan Thomas <jonathan@openshot.org>
 *
 * @ref License
 */

/* LICENSE
 *
 * Copyright (c) 2008-2019 OpenShot Studios, LLC
 * <http://www.openshotstudios.com/>. This file is part of
 * OpenShot Library (libopenshot), an open-source project dedicated to
 * delivering high quality video editing and animation solutions to the
 * world. For more information visit <http://www.openshot.org/>.
 *
 * OpenShot Library (libopenshot) is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * OpenShot Library (libopenshot) is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with OpenShot Library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "effects/Tracker.h"

using namespace openshot;

/// Blank constructor, useful when using Json to load the effect properties
Tracker::Tracker(std::string clipTrackerDataPath): delta_x(0.0), delta_y(0.0), scale_x(0.0), scale_y(0.0), rotation(0.0) 
{   
    // Init effect properties
	init_effect_details();
    // Tries to load the tracker data from protobuf
    LoadTrackedData(clipTrackerDataPath);
}

// Default constructor
Tracker::Tracker(): delta_x(0.0), delta_y(0.0), scale_x(0.0), scale_y(0.0), rotation(0.0) 
{
	// Init effect properties
	init_effect_details();

}


// Init effect settings
void Tracker::init_effect_details()
{
	/// Initialize the values of the EffectInfo struct.
	InitEffectInfo();

	/// Set the effect info
	info.class_name = "Tracker";
	info.name = "Tracker";
	info.description = "Track the selected bounding box through the video.";
	info.has_audio = false;
	info.has_video = true;

	this->TimeScale = 1.0;
}

// This method is required for all derived classes of EffectBase, and returns a
// modified openshot::Frame object
std::shared_ptr<Frame> Tracker::GetFrame(std::shared_ptr<Frame> frame, int64_t frame_number)
{

    // Get the frame's image
	cv::Mat frame_image = frame->GetImageCV();

    // Check if frame isn't NULL
    if(!frame_image.empty()){

        // Check if track data exists for the requested frame
        if (trackedData.Contains(frame_number)) {

			float fw = frame_image.size().width;
        	float fh = frame_image.size().height;

			double scale_x = this->scale_x.GetValue(frame_number);
			double scale_y = this->scale_y.GetValue(frame_number);
			double delta_x = this->delta_x.GetValue(frame_number);
			double delta_y = this->delta_y.GetValue(frame_number);


            // Draw box on image
            //EffectFrameData fd = trackedDataById[frame_number];

			BBox fd = this->trackedData.GetValue(frame_number);
 
            cv::Rect2d box((int)( (fd.cx + delta_x) * fw ),
						   (int)( (fd.cy + delta_y) * fh ),
						   (int)( (fd.width + scale_x) * fw),
						   (int)( (fd.height + scale_y) * fh) );
            cv::rectangle(frame_image, box, cv::Scalar( 255, 0, 0 ), 2, 1 );
        }
    }

	// Set image with drawn box to frame
    // If the input image is NULL or doesn't have tracking data, it's returned as it came
	frame->SetImageCV(frame_image);

	return frame;
}

// Load protobuf data file
bool Tracker::LoadTrackedData(std::string inputFilePath){
    // Create tracker message
    libopenshottracker::Tracker trackerMessage;

    {
        // Read the existing tracker message.
        fstream input(inputFilePath, ios::in | ios::binary);
        if (!trackerMessage.ParseFromIstream(&input)) {
            cerr << "Failed to parse protobuf message." << endl;
            return false;
        }
    }

    // Make sure the trackedData is empty
    //trackedDataById.clear();
	trackedData.clear();


    // Iterate over all frames of the saved message
    for (size_t i = 0; i < trackerMessage.frame_size(); i++) {
        const libopenshottracker::Frame& pbFrameData = trackerMessage.frame(i);

        // Load frame and rotation data
        size_t id = pbFrameData.id();
        float rotation = pbFrameData.rotation();

        // Load bounding box data
        const libopenshottracker::Frame::Box& box = pbFrameData.bounding_box();
        float x1 = box.x1();
        float y1 = box.y1();
        float x2 = box.x2();
        float y2 = box.y2();

        // Assign data to tracker map
        //trackedDataById[id] = EffectFrameData(id, rotation, x1, y1, x2, y2);
		if ((x1 >= 0.0) && (y1 >= 0.0) && (x2 >= 0.0) && (y2 >= 0.0)){
			trackedData.AddBox(id, x1, y1, (x2-x1), (y2-y1));
			//trackedData.AddRotation(id, rotation);
		}
	}

    // Show the time stamp from the last update in tracker data file 
    if (trackerMessage.has_last_updated()) {
        cout << "  Loaded Data. Saved Time Stamp: " << TimeUtil::ToString(trackerMessage.last_updated()) << endl;
    }

    // Delete all global objects allocated by libprotobuf.
    google::protobuf::ShutdownProtobufLibrary();

    return true;
}

// Get tracker info for the desired frame 
BBox Tracker::GetTrackedData(size_t frameId){
	return this->trackedData.GetValue(frameId);
}

// Generate JSON string of this object
std::string Tracker::Json() const {

	// Return formatted string
	return JsonValue().toStyledString();
}

// Generate Json::Value for this object
Json::Value Tracker::JsonValue() const {

	// Create root json object
	Json::Value root = EffectBase::JsonValue(); // get parent properties
	root["type"] = info.class_name;
	root["protobuf_data_path"] = protobuf_data_path;
    root["BaseFPS"]["num"] = BaseFPS.num;
	root["BaseFPS"]["den"] = BaseFPS.den;
	root["TimeScale"] = this->TimeScale;
    
	root["delta_x"] = delta_x.JsonValue();
    root["delta_y"] = delta_y.JsonValue();
    root["scale_x"] = scale_x.JsonValue();
    root["scale_y"] = scale_y.JsonValue(); 
    root["rotation"] = rotation.JsonValue();
	
	// return JsonValue
	return root;
}

// Load JSON string into this object
void Tracker::SetJson(const std::string value) {

	// Parse JSON string into JSON objects
	try
	{
		const Json::Value root = openshot::stringToJson(value);
		// Set all values that match
		SetJsonValue(root);
	}
	catch (const std::exception& e)
	{
		// Error parsing JSON (or missing keys)
		throw InvalidJSON("JSON is invalid (missing keys or invalid data types)");
	}
}

// Load Json::Value into this object
void Tracker::SetJsonValue(const Json::Value root) {

	// Set parent data
	EffectBase::SetJsonValue(root);
	
	if (!root["BaseFPS"].isNull() && root["BaseFPS"].isObject()) {
        if (!root["BaseFPS"]["num"].isNull())
			BaseFPS.num = (int) root["BaseFPS"]["num"].asInt();
        if (!root["BaseFPS"]["den"].isNull())
		    BaseFPS.den = (int) root["BaseFPS"]["den"].asInt();
	}
	
	if (!root["TimeScale"].isNull()){
		TimeScale = (double) root["TimeScale"].asDouble();
	}

	trackedData.SetBaseFPS(this->BaseFPS);
	trackedData.ScalePoints(TimeScale);

	// Set data from Json (if key is found)
	if (!root["protobuf_data_path"].isNull()){
		protobuf_data_path = (root["protobuf_data_path"].asString());
		
		if(!LoadTrackedData(protobuf_data_path)){
			std::cout<<"Invalid protobuf data path";
			protobuf_data_path = "";
		}
	}

	if (!root["delta_x"].isNull())
		delta_x.SetJsonValue(root["delta_x"]);
	if (!root["delta_y"].isNull())
		delta_y.SetJsonValue(root["delta_y"]);
	if (!root["scale_x"].isNull())
		scale_x.SetJsonValue(root["scale_x"]);
	if (!root["scale_y"].isNull())
		scale_y.SetJsonValue(root["scale_y"]);
	if (!root["rotation"].isNull())
		rotation.SetJsonValue(root["rotation"]);


}

// Get all properties for a specific frame
std::string Tracker::PropertiesJSON(int64_t requested_frame) const {

	// Generate JSON properties list
	Json::Value root;
	root["id"] = add_property_json("ID", 0.0, "string", Id(), NULL, -1, -1, true, requested_frame);
	root["position"] = add_property_json("Position", Position(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["layer"] = add_property_json("Track", Layer(), "int", "", NULL, 0, 20, false, requested_frame);
	root["start"] = add_property_json("Start", Start(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["end"] = add_property_json("End", End(), "float", "", NULL, 0, 1000 * 60 * 30, false, requested_frame);
	root["duration"] = add_property_json("Duration", Duration(), "float", "", NULL, 0, 1000 * 60 * 30, true, requested_frame);

	// Keyframes
	root["delta_x"] = add_property_json("Displacement X-axis", delta_x.GetValue(requested_frame), "float", "", &delta_x, -1.0, 1.0, false, requested_frame);
	root["delta_y"] = add_property_json("Displacement Y-axis", delta_y.GetValue(requested_frame), "float", "", &delta_y, -1.0, 1.0, false, requested_frame);
	root["scale_x"] = add_property_json("Scale (Width)", scale_x.GetValue(requested_frame), "float", "", &scale_x, -1.0, 1.0, false, requested_frame);
	root["scale_y"] = add_property_json("Scale (Height)", scale_y.GetValue(requested_frame), "float", "", &scale_y, -1.0, 1.0, false, requested_frame);
	root["rotation"] = add_property_json("Rotation", rotation.GetValue(requested_frame), "float", "", &rotation, 0, 360, false, requested_frame);

	// Return formatted string
	return root.toStyledString();
}