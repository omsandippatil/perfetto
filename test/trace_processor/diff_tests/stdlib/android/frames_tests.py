#!/usr/bin/env python3
# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License a
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from python.generators.diff_tests.testing import Path
from python.generators.diff_tests.testing import Csv, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class Frames(TestSuite):

  def test_android_frames_choreographer_do_frame(self):
    return DiffTestBlueprint(
        trace=Path('../../metrics/graphics/android_jank_cuj.py'),
        query="""
        INCLUDE PERFETTO MODULE android.frames.timeline;

        SELECT * FROM android_frames_choreographer_do_frame;
        """,
        out=Csv("""
        "id","frame_id"
        2,10
        15,20
        22,30
        35,40
        46,60
        55,90
        63,100
        73,110
        79,120
        87,130
        93,140
        99,145
        102,150
        108,160
        140,1000
        """))

  def test_android_frames_draw_frame(self):
    return DiffTestBlueprint(
        trace=Path('../../metrics/graphics/android_jank_cuj.py'),
        query="""
        INCLUDE PERFETTO MODULE android.frames.timeline;

        SELECT * FROM android_frames_draw_frame;
        """,
        out=Csv("""
      "id","frame_id"
      8,10
      16,20
      23,30
      41,40
      50,60
      57,90
      60,90
      66,100
      69,100
      74,110
      80,120
      89,130
      95,140
      100,145
      105,150
      109,160
      146,1000
        """))

  def test_android_frames(self):
    return DiffTestBlueprint(
        trace=Path('../../metrics/graphics/android_jank_cuj.py'),
        query="""
        INCLUDE PERFETTO MODULE android.frames.timeline;

        SELECT * FROM android_frames;
        """,
        out=Csv("""
        "frame_id","do_frame_id","draw_frame_id","actual_frame_timeline_id","expected_frame_timeline_id"
        10,2,8,1,0
        20,15,16,12,11
        30,22,23,21,20
        40,35,41,37,36
        60,46,50,48,47
        90,55,57,54,53
        90,55,60,54,53
        100,63,66,65,64
        100,63,69,65,64
        110,73,74,71,70
        120,79,80,78,77
        130,87,89,85,84
        140,93,95,94,91
        145,99,100,98,97
        150,102,105,104,103
        160,108,109,132,107
        1000,140,146,138,137
        """))

  def test_android_frames_overrun(self):
    return DiffTestBlueprint(
        trace=Path('../../metrics/graphics/android_jank_cuj.py'),
        query="""
        INCLUDE PERFETTO MODULE android.frames.per_frame_metrics;

        SELECT * FROM android_frames_overrun;
        """,
        out=Csv("""
        "frame_id","overrun"
        10,0
        20,-8000000
        30,-5000000
        40,-20000000
        60,10000000
        90,-3000000
        100,-2000000
        110,-41000000
        120,-41000000
        130,18000000
        140,-5600000
        145,0
        150,5000000
        160,-266000000
        190,0
        200,-16000000
        1000,-480000000
        """))

  def test_android_app_vsync_delay_per_frame(self):
    return DiffTestBlueprint(
        trace=Path('../../metrics/graphics/android_jank_cuj.py'),
        query="""
        INCLUDE PERFETTO MODULE android.frames.per_frame_metrics;

        SELECT * FROM android_app_vsync_delay_per_frame;
        """,
        out=Csv("""
        "frame_id","app_vsync_delay"
        10,0
        30,0
        40,0
        60,0
        90,0
        100,0
        110,0
        120,0
        140,100000
        150,500000
        160,270000000
        1000,0
        """))

  def test_android_cpu_time_per_frame(self):
    return DiffTestBlueprint(
        trace=Path('../../metrics/graphics/android_jank_cuj.py'),
        query="""
        INCLUDE PERFETTO MODULE android.frames.per_frame_metrics;

        SELECT * FROM android_cpu_time_per_frame;
        """,
        out=Csv("""
        "frame_id","app_vsync_delay","do_frame_dur","draw_frame_dur","cpu_time"
        10,0,5000000,1000000,6000000
        30,0,3000000,19000000,22000000
        40,0,13000000,7000000,20000000
        60,0,10000000,9000000,19000000
        90,0,15000000,8000000,23000000
        100,0,15000000,8000000,23000000
        110,0,15000000,2000000,17000000
        120,0,15000000,2000000,17000000
        140,100000,1500000,17000000,18600000
        150,500000,2000000,13800000,16300000
        160,270000000,2000000,1000000,273000000
        1000,0,100000000,150000000,250000000
        """))

  def test_android_frame_stats(self):
    return DiffTestBlueprint(
        trace=Path('../../metrics/graphics/android_jank_cuj.py'),
        query="""
        INCLUDE PERFETTO MODULE android.frames.per_frame_metrics;

        SELECT * FROM android_frame_stats;
        """,
        out=Csv("""
        "frame_id","overrun","cpu_time","ui_time","was_jank","was_slow_frame","was_big_jank","was_huge_jank"
        10,0,6000000,5000000,"[NULL]","[NULL]","[NULL]","[NULL]"
        20,-8000000,8000000,3000000,1,"[NULL]","[NULL]","[NULL]"
        30,-5000000,22000000,3000000,1,1,"[NULL]","[NULL]"
        40,-20000000,20000000,13000000,1,"[NULL]","[NULL]","[NULL]"
        60,10000000,19000000,10000000,"[NULL]","[NULL]","[NULL]","[NULL]"
        90,-3000000,23000000,15000000,1,1,"[NULL]","[NULL]"
        100,-2000000,23000000,15000000,1,1,"[NULL]","[NULL]"
        110,-41000000,17000000,15000000,1,"[NULL]","[NULL]","[NULL]"
        120,-41000000,17000000,15000000,1,"[NULL]","[NULL]","[NULL]"
        130,18000000,10000000,5000000,"[NULL]","[NULL]","[NULL]","[NULL]"
        140,-5600000,18600000,1500000,1,"[NULL]","[NULL]","[NULL]"
        145,0,25000000,20000000,"[NULL]",1,"[NULL]","[NULL]"
        150,5000000,16300000,2000000,"[NULL]","[NULL]","[NULL]","[NULL]"
        160,-266000000,273000000,2000000,1,1,1,1
        1000,-480000000,250000000,100000000,1,1,1,1
        """))
