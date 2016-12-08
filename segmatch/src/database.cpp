#include "segmatch/database.hpp"

#include <fstream>

#include <boost/filesystem.hpp>
#include <glog/logging.h>

namespace segmatch {

bool IdMatches::findMatches(const Id id, std::vector<Id>* matches) const {
  CHECK_NOTNULL(matches)->clear();
  Position position;
  if (findId(id, &position)) {
    // Copy the matches and remove oneself.
    *matches = id_match_list_.at(position.row);
    matches->erase(matches->begin() + position.col);
    return true;
  }
  return false;
}

bool IdMatches::areIdsMatching(const Id id1, const Id id2) const {
  Position position1;
  Position position2;
  if (findId(id1, &position1) && findId(id2, &position2)) {
    return position1.row == position2.row;
  }
  return false;
}

void IdMatches::addMatch(const Id id1, const Id id2) {
  CHECK_NE(id1, id2) << "No point in adding match between identical ids.";
  Position position1;
  Position position2;
  if (findId(id1, &position1)) {
    if (findId(id2, &position2)) {
      // Found both.
      if (position1.row != position2.row) {
        // Each id is already in separate groups -> concatenate both groups into one.
        std::vector<Id> matches2_copy = id_match_list_.at(position2.row);
        id_match_list_.erase(id_match_list_.begin() + position2.row);
        CHECK(findId(id1, &position1));
        id_match_list_.at(position1.row).insert(id_match_list_.at(position1.row).begin(),
                                               matches2_copy.begin(), matches2_copy.end());
        return;
      } else {
        // Match already exists.
        return;
      }
    } else {
      // Found id1 but not id2 -> add id1 to id2's matches' group.
      id_match_list_.at(position1.row).push_back(id2);
      return;
    }
  } else if (findId(id2, &position2)) {
    // Found id2 but not id1 -> add id1 to id2's matches' group.
    id_match_list_.at(position2.row).push_back(id1);
    return;
  } else {
    // Found neither.
    std::vector<Id> match_to_add;
    match_to_add.push_back(id1);
    match_to_add.push_back(id2);
    id_match_list_.push_back(match_to_add);
    return;
  }
}

void IdMatches::clear() {
  id_match_list_.clear();
}

std::string IdMatches::asString() const {
  std::stringstream result;
  for (size_t i = 0u; i < id_match_list_.size(); ++i) {
    for (size_t j = 0u; j < id_match_list_.at(i).size(); ++j) {
    result << id_match_list_.at(i).at(j) << " ";
    }
    result << std::endl;
  }
  return result.str();
}

bool IdMatches::findId(const Id id, Position* position_ptr) const {
  for (size_t i = 0u; i < id_match_list_.size(); ++i) {
    for (size_t j = 0u; j < id_match_list_.at(i).size(); ++j) {
      if (id == id_match_list_.at(i).at(j)) {
        // If desired, pass the found id's position.
        if (position_ptr != NULL) {
          position_ptr->row = i;
          position_ptr->col = j;
        }
        return true;
      }
    }
  }
  return false;
}

} // namespace segmatch

using namespace segmatch;

const std::string kDatabaseDirectory = "/tmp/segmatch/";
const std::string kSegmentsFilename = "segments_database.csv";
const std::string kFeaturesFilename = "features_database.csv";
const std::string kMatchesFilename = "matches_database.csv";

bool export_session_data_to_database(const SegmentedCloud& segmented_cloud,
                                     const IdMatches& id_matches) {
  return export_segments(kDatabaseDirectory + kSegmentsFilename, segmented_cloud) &&
      export_features(kDatabaseDirectory + kFeaturesFilename, segmented_cloud) &&
      export_matches(kDatabaseDirectory + kMatchesFilename, id_matches);
}

bool import_session_data_from_database(SegmentedCloud* segmented_cloud_ptr,
                                       IdMatches* id_matches_ptr) {
  return import_segments(kDatabaseDirectory + kSegmentsFilename, segmented_cloud_ptr) &&
      import_features(kDatabaseDirectory + kFeaturesFilename, segmented_cloud_ptr) &&
      import_matches(kDatabaseDirectory + kMatchesFilename, id_matches_ptr);
}

bool ensure_directory_exists(const std::string& directory) {
  CHECK(directory.size() != 0u) << "Directory should not be an empty string.";
  if (directory[0] == '/') {
    boost::filesystem::path path(directory);
    if(boost::filesystem::exists(path)) {
      return true;
    }
    if(boost::filesystem::create_directory(path)) {
      LOG(WARNING) << "Directory Created: " << directory;
      return true;
    }
  } else {
    LOG(ERROR) << "Directory '" << directory << "' starts with invalid character: '" <<
        directory[0] << "'"; 
  }
  return false;
}

bool ensure_directory_exists_for_filename(const std::string& filename) {
  size_t pos = filename.rfind("/");
  if (pos != std::string::npos) {
    std::string directory = filename.substr(0, pos);
    return ensure_directory_exists(directory);
  } else {
    LOG(ERROR) << "Filename '" << filename << "' does not specify a directory.";
    return false;
  }
}

bool export_segments(const std::string& filename, const SegmentedCloud& segmented_cloud) {
  ensure_directory_exists_for_filename(filename);
  std::ofstream output_file;
  output_file.open(filename, std::ofstream::out | std::ofstream::trunc);
  if (output_file.is_open()) {
    for (size_t i = 0u; i < segmented_cloud.getNumberOfValidSegments(); i++) {
      Segment segment = segmented_cloud.getValidSegmentByIndex(i);
      for (size_t j = 0u; j < segment.point_cloud.size(); ++j) {
        output_file << segment.segment_id << " ";
        output_file << segment.point_cloud.points[j].x << " ";
        output_file << segment.point_cloud.points[j].y << " ";
        output_file << segment.point_cloud.points[j].z;
        output_file << std::endl;
      }
    }
    output_file.close();
    LOG(INFO) << segmented_cloud.getNumberOfValidSegments() << " segments written to " << filename;
    return true;
  } else {
    LOG(ERROR) << "Could not open file " << filename << " for writing segments.";
    return false;
  }
}

bool export_features(const std::string& filename, const SegmentedCloud& segmented_cloud) {
  ensure_directory_exists_for_filename(filename);
  std::ofstream output_file;
  output_file.open(filename, std::ofstream::out | std::ofstream::trunc);
  if (output_file.is_open()) {
    for (size_t i = 0u; i < segmented_cloud.getNumberOfValidSegments(); i++) {
      Segment segment = segmented_cloud.getValidSegmentByIndex(i);
      output_file << segment.segment_id << " ";
      std::vector<FeatureValueType> values = segment.features.asVectorOfValues();
      std::vector<std::string> names = segment.features.asVectorOfNames();
      for (size_t j = 0u; j < values.size(); ++j) {
        output_file << names.at(j) << " ";
        output_file << values.at(j) << " ";
      }
      output_file << std::endl;
    }
    output_file.close();
    LOG(INFO) << "Features written to " << filename;
    return true;
  } else {
    LOG(ERROR) << "Could not open file " << filename << " for writing features.";
    return false;
  }
}

bool export_features_and_centroids(const std::string& filename,
                                   const segmatch::SegmentedCloud& segmented_cloud) {
  ensure_directory_exists_for_filename(filename);
  std::ofstream output_file;
  output_file.open(filename, std::ofstream::out | std::ofstream::trunc);
  if (output_file.is_open()) {
    for (size_t i = 0u; i < segmented_cloud.getNumberOfValidSegments(); i++) {
      Segment segment = segmented_cloud.getValidSegmentByIndex(i);

      output_file << segment.centroid.x << ", ";
      output_file << segment.centroid.y << ", ";
      output_file << segment.centroid.z << ", ";

      std::vector<FeatureValueType> values = segment.features.asVectorOfValues();
      for (size_t j = 0u; j < values.size(); ++j) {
        output_file << values.at(j);
        if (j < values.size() - 1u) {
          output_file << ", ";
        } else {
          output_file << std::endl;
        }
      }
    }
    output_file.close();
    LOG(INFO) << "Features written to " << filename;
    return true;
  } else {
    LOG(ERROR) << "Could not open file " << filename << " for writing features.";
    return false;
  }
}

bool export_matches(const std::string& filename, const IdMatches& matches) {
  ensure_directory_exists_for_filename(filename);
  std::ofstream output_file;
  output_file.open(filename, std::ofstream::out | std::ofstream::trunc);
  if (output_file.is_open()) {
    for (size_t i = 0u; i < matches.size(); i++) {
      for (size_t j = 0u; j < matches.at(i).size(); ++j) {
        output_file << matches.at(i).at(j) << " ";
      }
      output_file << std::endl;
    }
    output_file.close();
    LOG(INFO) << "Matches written to " << filename;
    return true;
  } else {
    LOG(ERROR) << "Could not open file " << filename << " for writing matches.";
    return false;
  }
}

bool import_segments(const std::string& filename, SegmentedCloud* segmented_cloud_ptr) {
  CHECK_NOTNULL(segmented_cloud_ptr);
  std::ifstream input_file;
  input_file.open(filename);
  size_t segments_count = 0u;
  if (input_file.good()) {
    // Get the current line.
    std::string line;
    Segment segment;
    while(getline(input_file, line)) {
      std::istringstream line_as_stream(line);
      // Read first number as segment id.
      Id line_id;
      line_as_stream >> line_id;
      // If id has changed, save segment and create new one.
      if (line_id != segment.segment_id && !segment.empty()) {
        // Ensure that ids remain unique.
        if (segmented_cloud_ptr->findValidSegmentPtrById(segment.segment_id, NULL)) {
          LOG(WARNING) << "Did not import segment of id " << segment.segment_id <<
            ". A segment with that id already exists.";
        } else {
          segmented_cloud_ptr->addValidSegment(segment);
          ++segments_count;
        }
        segment.clear();
      }
      segment.segment_id = line_id;
      PointI point;
      line_as_stream >> point.x;
      line_as_stream >> point.y;
      line_as_stream >> point.z;
      segment.point_cloud.push_back(point);
    }
    // After the loop: Store the last segment.
    if (segment.hasValidId()) {
      if (segmented_cloud_ptr->findValidSegmentPtrById(segment.segment_id, NULL)) {
        LOG(WARNING) << "Did not import segment of id " << segment.segment_id <<
          ". A segment with that id already exists.";
      } else {
        segmented_cloud_ptr->addValidSegment(segment);
        ++segments_count;
      }
    }
    input_file.close();
    LOG(INFO) << "Imported " << segments_count << " segments from file " << filename;
    return true;
  } else {
    LOG(ERROR) << "Could not open file " << filename << " for importing segments.";
    return false;
  }
}

bool import_features(const std::string& filename, SegmentedCloud* segmented_cloud_ptr,
                     const std::string& behavior_when_segment_has_features) {
  CHECK_NOTNULL(segmented_cloud_ptr);
  std::ifstream input_file;
  input_file.open(filename);
  size_t segments_count = 0u;
  if (input_file.good()) {
    // Get the current line.
    std::string line;
    while(getline(input_file, line)) {
      std::istringstream line_as_stream(line);
      // Read first number as segment id.
      Id segment_id;
      CHECK(line_as_stream >> segment_id) << "Could not read Id.";
      Segment* segment_ptr;
      if (!segmented_cloud_ptr->findValidSegmentPtrById(segment_id, &segment_ptr)) {
        LOG(ERROR) << "Could not find segment of id " << segment_id <<
            " when importing features for that id.";
      } else {
        // Read features.
        Features features;
        std::string name;
        while(line_as_stream >> name) {
          FeatureValueType value;
          line_as_stream >> value;
          features.push_back(Feature(name, value));
        }
        // Check wether segment already has features.
        if (!segment_ptr->features.empty()) {
          if (behavior_when_segment_has_features == "concatenate") {
            segment_ptr->features += features;
          } else if (behavior_when_segment_has_features == "replace") {
            segment_ptr->features = features;
          }else /* behavior_when_segment_has_features == "abort" */ {
            LOG(FATAL) << "Segment " << segment_id <<
                " into which features are being imported already has features, " <<
                "and tolerance has not been set. Aborting.";
          }
        } else {
          segment_ptr->features = features;
        }
        ++segments_count;
      }
    }
    input_file.close();
    LOG(INFO) << "Imported features for " << segments_count << " segments from file " << filename;
    return true;
  } else {
    LOG(ERROR) << "Could not open file " << filename << " for importing features.";
    return false;
  }
}

bool import_matches(const std::string& filename, IdMatches* matches_ptr) {
  CHECK_NOTNULL(matches_ptr);
  if (!matches_ptr->empty()) {
    LOG(ERROR) << "Should not import matches into non-empty IdMatches object.";
    return false;
  }
  std::ifstream input_file;
  input_file.open(filename);
  size_t matches_count = 0u;
  if (input_file.good()) {
    // Get the current line.
    std::string line;
    std::vector<std::vector<Id> > matches_vector;
    while(getline(input_file, line)) {
      std::istringstream line_as_stream(line);
      Id id;
      std::vector<Id> match_group;
      while(line_as_stream >> id) {
        match_group.push_back(id);
      }
      matches_vector.push_back(match_group);
      ++matches_count;
    }
    *matches_ptr = IdMatches(matches_vector);
    input_file.close();
    LOG(INFO) << "Imported " << matches_count << " matches from file " << filename;
    return true;
  } else {
    LOG(ERROR) << "Could not open file " << filename << " for importing matches.";
    return false;
  }
}
