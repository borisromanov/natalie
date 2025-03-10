require_relative '../../spec_helper'
require 'rbconfig'

describe 'RbConfig::CONFIG' do
  it 'values are all strings' do
    RbConfig::CONFIG.each do |k, v|
      k.should be_kind_of String
      v.should be_kind_of String
    end
  end

  # These directories have no meanings before the installation.
  guard -> { RbConfig::TOPDIR } do
    it "['rubylibdir'] returns the directory containing Ruby standard libraries" do
      rubylibdir = RbConfig::CONFIG['rubylibdir']
      File.directory?(rubylibdir).should == true
      File.should.exist?("#{rubylibdir}/fileutils.rb")
    end

    it "['archdir'] returns the directory containing standard libraries C extensions" do
      archdir = RbConfig::CONFIG['archdir']
      File.directory?(archdir).should == true
      File.should.exist?("#{archdir}/etc.#{RbConfig::CONFIG['DLEXT']}")
    end

    it "['sitelibdir'] is set and is part of $LOAD_PATH" do
      sitelibdir = RbConfig::CONFIG['sitelibdir']
      sitelibdir.should be_kind_of String
      $LOAD_PATH.map{|path| File.realpath(path) rescue path }.should.include? sitelibdir
    end
  end
  # NATFIXME: Pending support for frozen-string-literal
  xit "contains no frozen strings even with --enable-frozen-string-literal" do
    ruby_exe(<<-RUBY, options: '--enable-frozen-string-literal').should == "Done\n"
      require 'rbconfig'
      RbConfig::CONFIG.each do |k, v|
        if v.frozen?
          puts "\#{k} Failure"
        end
      end
      puts 'Done'
    RUBY
  end

  platform_is_not :windows do
    it "['LIBRUBY'] is the same as LIBRUBY_SO if and only if ENABLE_SHARED" do
      case RbConfig::CONFIG['ENABLE_SHARED']
      when 'yes'
        RbConfig::CONFIG['LIBRUBY'].should == RbConfig::CONFIG['LIBRUBY_SO']
      when 'no'
        RbConfig::CONFIG['LIBRUBY'].should_not == RbConfig::CONFIG['LIBRUBY_SO']
      end
    end
  end

  guard -> { RbConfig::TOPDIR } do
    it "libdir/LIBRUBY_SO is the path to libruby and it exists if and only if ENABLE_SHARED" do
      libdirname = RbConfig::CONFIG['LIBPATHENV'] == 'PATH' ? 'bindir' :
                     RbConfig::CONFIG['libdirname']
      libdir = RbConfig::CONFIG[libdirname]
      libruby_so = "#{libdir}/#{RbConfig::CONFIG['LIBRUBY_SO']}"
      case RbConfig::CONFIG['ENABLE_SHARED']
      when 'yes'
        File.should.exist?(libruby_so)
      when 'no'
        File.should_not.exist?(libruby_so)
      end
    end
  end

  platform_is :linux do
    it "['AR'] exists and can be executed" do
      ar = RbConfig::CONFIG.fetch('AR')
      out = `#{ar} --version`
      $?.should.success?
      out.should_not be_empty
    end

    # NATFIXME: cp helper not yet implemented
    xit "['STRIP'] exists and can be executed" do
      strip = RbConfig::CONFIG.fetch('STRIP')
      copy = tmp("sh")
      cp '/bin/sh', copy
      begin
        out = `#{strip} #{copy}`
        $?.should.success?
      ensure
        rm_r copy
      end
    end
  end
end

describe "RbConfig::TOPDIR" do
  it "either returns nil (if not installed) or the prefix" do
    if RbConfig::TOPDIR
      RbConfig::TOPDIR.should == RbConfig::CONFIG["prefix"]
    else
      RbConfig::TOPDIR.should == nil
    end
  end
end
