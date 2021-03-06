/*
  This source is part of the libosmscout library
  Copyright (C) 2013  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <osmscout/Area.h>

#include <algorithm>
#include <limits>

#include <osmscout/util/String.h>

#include <osmscout/system/Math.h>

namespace osmscout {

  bool Area::Ring::GetCenter(GeoCoord& center) const
  {
    double minLat=0.0;
    double minLon=0.0;
    double maxLat=0.0;
    double maxLon=0.0;

    bool start=true;

    for (size_t j=0; j<nodes.size(); j++) {
      if (start) {
        minLat=nodes[j].GetLat();
        minLon=nodes[j].GetLon();
        maxLat=nodes[j].GetLat();
        maxLon=nodes[j].GetLon();

        start=false;
      }
      else {
        minLat=std::min(minLat,nodes[j].GetLat());
        minLon=std::min(minLon,nodes[j].GetLon());
        maxLat=std::max(maxLat,nodes[j].GetLat());
        maxLon=std::max(maxLon,nodes[j].GetLon());
      }
    }

    if (start) {
      return false;
    }

    center.Set(minLat+(maxLat-minLat)/2,
               minLon+(maxLon-minLon)/2);

    return true;
  }

  void Area::Ring::GetBoundingBox(GeoBox& boundingBox) const
  {
    assert(!nodes.empty());

    double minLon=nodes[0].GetLon();
    double maxLon=minLon;
    double minLat=nodes[0].GetLat();
    double maxLat=minLat;

    for (size_t i=1; i<nodes.size(); i++) {
      minLon=std::min(minLon,nodes[i].GetLon());
      maxLon=std::max(maxLon,nodes[i].GetLon());
      minLat=std::min(minLat,nodes[i].GetLat());
      maxLat=std::max(maxLat,nodes[i].GetLat());
    }

    boundingBox.Set(GeoCoord(minLat,minLon),
                    GeoCoord(maxLat,maxLon));
  }

  bool Area::GetCenter(GeoCoord& center) const
  {
    assert(!rings.empty());

    double minLat=0.0;
    double minLon=0.0;
    double maxLat=0.0;
    double maxLon=0.0;

    bool start=true;

    for (const auto& ring : rings) {
      if (ring.ring==Area::outerRingId) {
        for (size_t j=0; j<ring.nodes.size(); j++) {
          if (start) {
            minLat=ring.nodes[j].GetLat();
            maxLat=minLat;
            minLon=ring.nodes[j].GetLon();
            maxLon=minLon;

            start=false;
          }
          else {
            minLat=std::min(minLat,ring.nodes[j].GetLat());
            minLon=std::min(minLon,ring.nodes[j].GetLon());
            maxLat=std::max(maxLat,ring.nodes[j].GetLat());
            maxLon=std::max(maxLon,ring.nodes[j].GetLon());
          }
        }
      }
    }

    assert(!start);

    if (start) {
      return false;
    }

    center.Set(minLat+(maxLat-minLat)/2,
               minLon+(maxLon-minLon)/2);

    return true;
  }

  void Area::GetBoundingBox(GeoBox& boundingBox) const
  {
    boundingBox.Invalidate();

    for (const auto& role : rings) {
      if (role.ring==Area::outerRingId) {
        if (!boundingBox.IsValid()) {
          role.GetBoundingBox(boundingBox);
        }
        else {
          GeoBox ringBoundingBox;

          role.GetBoundingBox(ringBoundingBox);

          boundingBox.Include(ringBoundingBox);
        }
      }
    }
  }

  void Area::ReadIds(FileScanner& scanner,
                     uint32_t nodesCount,
                     std::vector<Id>& ids)
  {
    ids.resize(nodesCount);

    Id minId;

    scanner.ReadNumber(minId);

    if (minId>0) {
      size_t idCurrent=0;

      while (idCurrent<ids.size()) {
        uint8_t bitset;
        size_t  bitmask=1;

        scanner.Read(bitset);

        for (size_t i=0; i<8 && idCurrent<ids.size(); i++) {
          if (bitset & bitmask) {
            scanner.ReadNumber(ids[idCurrent]);

            ids[idCurrent]+=minId;
          }
          else {
            ids[idCurrent]=0;
          }

          bitmask*=2;
          idCurrent++;
        }
      }
    }
  }

  /**
   * Reads data from the given Filescanner. Node ids will only be read
   * if not thought to be required for this area.
   *
   * @throws IOException
   */
  void Area::Read(const TypeConfig& typeConfig,
                  FileScanner& scanner)
  {
    TypeId             ringType;
    bool               multipleRings;
    uint32_t           ringCount=1;
    FeatureValueBuffer featureValueBuffer;

    fileOffset=scanner.GetPos();

    scanner.ReadTypeId(ringType,
                       typeConfig.GetAreaTypeIdBytes());

    TypeInfoRef type=typeConfig.GetAreaTypeInfo(ringType);

    featureValueBuffer.SetType(type);

    featureValueBuffer.Read(scanner,
                            multipleRings);

    if (multipleRings) {
      scanner.ReadNumber(ringCount);

      ringCount++;
    }

    rings.resize(ringCount);

    rings[0].featureValueBuffer=std::move(featureValueBuffer);

    if (ringCount>1) {
      rings[0].ring=masterRingId;
    }
    else {
      rings[0].ring=outerRingId;
    }

    scanner.Read(rings[0].nodes);

    if (!rings[0].nodes.empty() &&
        rings[0].GetType()->CanRoute()) {
      ReadIds(scanner,
              rings[0].nodes.size(),
              rings[0].ids);
    }

    for (size_t i=1; i<ringCount; i++) {
      scanner.ReadTypeId(ringType,
                         typeConfig.GetAreaTypeIdBytes());

      type=typeConfig.GetAreaTypeInfo(ringType);

      rings[i].SetType(type);

      if (rings[i].GetType()->GetAreaId()!=typeIgnore) {
        rings[i].featureValueBuffer.Read(scanner);
      }

      scanner.Read(rings[i].ring);
      scanner.Read(rings[i].nodes);

      if (!rings[i].nodes.empty() &&
          rings[i].GetType()->GetAreaId()!=typeIgnore &&
          rings[i].GetType()->CanRoute()) {
        ReadIds(scanner,
                rings[i].nodes.size(),
                rings[i].ids);
      }
    }
  }

  /**
   * Reads data from the given FileScanner. All data available will be read.
   *
   * @throws IOException
   */
  void Area::ReadImport(const TypeConfig& typeConfig,
                        FileScanner& scanner)
  {
    TypeId             ringType;
    bool               multipleRings;
    uint32_t           ringCount=1;
    FeatureValueBuffer featureValueBuffer;

    fileOffset=scanner.GetPos();

    scanner.ReadTypeId(ringType,
                       typeConfig.GetAreaTypeIdBytes());

    TypeInfoRef type=typeConfig.GetAreaTypeInfo(ringType);

    featureValueBuffer.SetType(type);

    featureValueBuffer.Read(scanner,
                            multipleRings);

    if (multipleRings) {
      scanner.ReadNumber(ringCount);

      ringCount++;
    }

    rings.resize(ringCount);

    rings[0].featureValueBuffer=featureValueBuffer;

    if (ringCount>1) {
      rings[0].ring=masterRingId;
    }
    else {
      rings[0].ring=outerRingId;
    }

    scanner.Read(rings[0].nodes);

    if (!rings[0].nodes.empty()) {
      ReadIds(scanner,
              rings[0].nodes.size(),
              rings[0].ids);
    }

    for (size_t i=1; i<ringCount; i++) {
      scanner.ReadTypeId(ringType,
                         typeConfig.GetAreaTypeIdBytes());

      type=typeConfig.GetAreaTypeInfo(ringType);

      rings[i].SetType(type);

      if (rings[i].GetType()->GetAreaId()!=typeIgnore) {
        rings[i].featureValueBuffer.Read(scanner);
      }

      scanner.Read(rings[i].ring);
      scanner.Read(rings[i].nodes);

      if (!rings[i].nodes.empty() &&
          rings[i].GetType()->GetAreaId()!=typeIgnore) {
        ReadIds(scanner,
                rings[i].nodes.size(),
                rings[i].ids);
      }
    }
  }

  /**
   * Reads data to the given FileScanner. No node ids will be read.
   *
   * @throws IOException
   */
  void Area::ReadOptimized(const TypeConfig& typeConfig,
                           FileScanner& scanner)
  {
    TypeId             ringType;
    bool               multipleRings;
    uint32_t           ringCount=1;
    FeatureValueBuffer featureValueBuffer;

    fileOffset=scanner.GetPos();

    scanner.ReadTypeId(ringType,
                       typeConfig.GetAreaTypeIdBytes());

    TypeInfoRef type=typeConfig.GetAreaTypeInfo(ringType);

    featureValueBuffer.SetType(type);

    featureValueBuffer.Read(scanner,
                            multipleRings);

    if (multipleRings) {
      scanner.ReadNumber(ringCount);

      ringCount++;
    }

    rings.resize(ringCount);

    rings[0].featureValueBuffer=featureValueBuffer;

    if (ringCount>1) {
      rings[0].ring=masterRingId;
    }
    else {
      rings[0].ring=outerRingId;
    }

    scanner.Read(rings[0].nodes);

    for (size_t i=1; i<ringCount; i++) {
      scanner.ReadTypeId(ringType,
                         typeConfig.GetAreaTypeIdBytes());

      type=typeConfig.GetAreaTypeInfo(ringType);

      rings[i].SetType(type);

      if (rings[i].featureValueBuffer.GetType()->GetAreaId()!=typeIgnore) {
        rings[i].featureValueBuffer.Read(scanner);
      }

      scanner.Read(rings[i].ring);
      scanner.Read(rings[i].nodes);
    }
  }

  void Area::WriteIds(FileWriter& writer,
                      const std::vector<Id>& ids) const
  {
    Id minId=0;

    for (size_t i=0; i<ids.size(); i++) {
      if (ids[i]!=0) {
        if (minId==0) {
          minId=ids[i];
        }
        else {
          minId=std::min(minId,ids[i]);
        }
      }
    }

    writer.WriteNumber(minId);

    if (minId>0) {
      size_t idCurrent=0;

      while (idCurrent<ids.size()) {
        uint8_t bitset=0;
        uint8_t bitMask=1;
        size_t  idEnd=std::min(idCurrent+8,ids.size());

        for (size_t i=idCurrent; i<idEnd; i++) {
          if (ids[i]!=0) {
            bitset=bitset | bitMask;
          }

          bitMask*=2;
        }

        writer.Write(bitset);

        for (size_t i=idCurrent; i<idEnd; i++) {
          if (ids[i]!=0) {
            writer.WriteNumber(ids[i]-minId);
          }

          bitMask=bitMask*2;
        }

        idCurrent+=8;
      }
    }
  }

  /**
   * Writes data to the given FileWriter. Node ids will only be written
   * if not thought to be required for this area.
   *
   * @throws IOException
   */
  void Area::Write(const TypeConfig& typeConfig,
                   FileWriter& writer) const
  {
    std::vector<Ring>::const_iterator ring=rings.begin();
    bool                              multipleRings=rings.size()>1;

    // Outer ring

    writer.WriteTypeId(ring->GetType()->GetAreaId(),
                       typeConfig.GetAreaTypeIdBytes());

    ring->featureValueBuffer.Write(writer,
                                   multipleRings);

    if (multipleRings) {
      writer.WriteNumber((uint32_t)(rings.size()-1));
    }

    writer.Write(ring->nodes);

    if (!ring->nodes.empty() &&
        ring->GetType()->CanRoute()) {
      WriteIds(writer,
               ring->ids);
    }

    ++ring;

    // Potential additional rings

    while (ring!=rings.end()) {
      writer.WriteTypeId(ring->GetType()->GetAreaId(),
                         typeConfig.GetAreaTypeIdBytes());

      if (ring->GetType()->GetAreaId()!=typeIgnore) {
        ring->featureValueBuffer.Write(writer);
      }

      writer.Write(ring->ring);
      writer.Write(ring->nodes);

      if (!ring->nodes.empty() &&
          ring->GetType()->GetAreaId()!=typeIgnore &&
          ring->GetType()->CanRoute()) {
        WriteIds(writer,
                 ring->ids);
      }

      ++ring;
    }
  }

  /**
   * Writes data to the given FileWriter. All data available will be written.
   *
   * @throws IOException
   */
  void Area::WriteImport(const TypeConfig& typeConfig,
                         FileWriter& writer) const
  {
    std::vector<Ring>::const_iterator ring=rings.begin();
    bool                              multipleRings=rings.size()>1;

    // Outer ring

    writer.WriteTypeId(ring->GetType()->GetAreaId(),
                       typeConfig.GetAreaTypeIdBytes());

    ring->featureValueBuffer.Write(writer,
                                   multipleRings);

    if (multipleRings) {
      writer.WriteNumber((uint32_t)(rings.size()-1));
    }

    writer.Write(ring->nodes);

    if (!ring->nodes.empty()) {
      WriteIds(writer,
               ring->ids);
    }

    ++ring;

    // Potential additional rings

    while (ring!=rings.end()) {
      writer.WriteTypeId(ring->GetType()->GetAreaId(),
                         typeConfig.GetAreaTypeIdBytes());

      if (ring->GetType()->GetAreaId()!=typeIgnore) {
        ring->featureValueBuffer.Write(writer);
      }

      writer.Write(ring->ring);
      writer.Write(ring->nodes);

      if (!ring->nodes.empty() &&
          ring->GetType()->GetAreaId()!=typeIgnore) {
        WriteIds(writer,
                 ring->ids);
      }

      ++ring;
    }
  }

  /**
   * Writes data to the given FileWriter. No node ids will be written.
   *
   * @throws IOException
   */
  void Area::WriteOptimized(const TypeConfig& typeConfig,
                            FileWriter& writer) const
  {
    std::vector<Ring>::const_iterator ring=rings.begin();
    bool                              multipleRings=rings.size()>1;

    // Outer ring

    writer.WriteTypeId(ring->GetType()->GetAreaId(),
                       typeConfig.GetAreaTypeIdBytes());

    ring->featureValueBuffer.Write(writer,
                                   multipleRings);

    if (multipleRings) {
      writer.WriteNumber((uint32_t)(rings.size()-1));
    }

    writer.Write(ring->nodes);

    ++ring;

    // Potential additional rings

    while (ring!=rings.end()) {
      writer.WriteTypeId(ring->GetType()->GetAreaId(),
                         typeConfig.GetAreaTypeIdBytes());

      if (ring->GetType()->GetAreaId()!=typeIgnore) {
        ring->featureValueBuffer.Write(writer);
      }

      writer.Write(ring->ring);
      writer.Write(ring->nodes);

      ++ring;
    }
  }
}

