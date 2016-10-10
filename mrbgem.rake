require 'open3'
require 'fileutils'

MRuby::Gem::Specification.new 'mruby-tinycc' do |s|
  s.license = 'MIT'
  s.author = 'Takeshi Watanabe'
  s.summary = 'libtcc binding for mruby'

  tinycc_dir = "#{build_dir}/tinycc"
  header = "#{tinycc_dir}/libtcc.h"
  tinycc_lib = libfile "#{tinycc_dir}/libtcc"
  tinycc_objs_dir = "#{tinycc_dir}/objs"
  libmruby_a = libfile "#{build.build_dir}/lib/libmruby"

  task :clean do
    FileUtils.rm_rf [tinycc_dir]
  end

  file header do
    FileUtils.mkdir_p tinycc_dir
    Dir.chdir build_dir do
      git.run_clone tinycc_dir, 'git://repo.or.cz/tinycc.git', '--depth 1'
    end
  end

  def run_command(env, command)
    STDOUT.sync = true
    Open3.popen2e(env, command) do |stdin, stdout, thread|
      print stdout.read
      fail "#{command} failed" if thread.value != 0
    end
  end

  file tinycc_lib => header do
    Dir.chdir tinycc_dir do
      e = {}

      run_command e, './configure'
      run_command e, 'make XAR=""'
    end

    FileUtils.mkdir_p tinycc_objs_dir
    FileUtils.cp Dir.glob("#{tinycc_dir}/*.o"), tinycc_objs_dir, verbose: true
    file libmruby_a => Dir.glob("#{tinycc_objs_dir}/*.o")
  end

  file libmruby_a => Dir.glob("#{tinycc_objs_dir}/*.o") if File.exists? tinycc_lib

  file "#{dir}/src/mrb_tinycc.c" => tinycc_lib
  s.cc.include_paths << tinycc_dir
end
