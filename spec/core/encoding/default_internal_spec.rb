require_relative '../../spec_helper'

describe "Encoding.default_internal" do
  before :each do
    @original_encoding = Encoding.default_internal
  end

  after :each do
    Encoding.default_internal = @original_encoding
  end

  it "is nil by default" do
    Encoding.default_internal.should be_nil
  end

  it "returns an Encoding object if a default internal encoding is set" do
    Encoding.default_internal = Encoding::ASCII
    Encoding.default_internal.should be_an_instance_of(Encoding)
  end

  it "returns nil if no default internal encoding is set" do
    Encoding.default_internal = nil
    Encoding.default_internal.should be_nil
  end

  it "returns the default internal encoding" do
    Encoding.default_internal = Encoding::BINARY
    Encoding.default_internal.should == Encoding::BINARY
  end
end

describe "Encoding.default_internal=" do
  before :each do
    @original_encoding = Encoding.default_internal
  end

  after :each do
    Encoding.default_internal = @original_encoding
  end

  # NATFIXME: Implement Encoding::SHIFT_JIS
  xit "sets the default internal encoding" do
    Encoding.default_internal = Encoding::SHIFT_JIS
    Encoding.default_internal.should == Encoding::SHIFT_JIS
  end

  # NATFIXME: Implement Encoding::SHIFT_JIS
  xit "can accept a name of an encoding as a String" do
    Encoding.default_internal = 'Shift_JIS'
    Encoding.default_internal.should == Encoding::SHIFT_JIS
  end

  it "calls #to_str to convert an object to a String" do
    obj = mock('string')
    obj.should_receive(:to_str).at_least(1).times.and_return('ascii')

    Encoding.default_internal = obj
    Encoding.default_internal.should == Encoding::ASCII
  end

  it "raises a TypeError if #to_str does not return a String" do
    obj = mock('string')
    obj.should_receive(:to_str).at_least(1).times.and_return(1)

    -> { Encoding.default_internal = obj }.should raise_error(TypeError)
  end

  it "raises a TypeError when passed an object not providing #to_str" do
    -> { Encoding.default_internal = mock("encoding") }.should raise_error(TypeError)
  end

  it "accepts an argument of nil to unset the default internal encoding" do
    Encoding.default_internal = nil
    Encoding.default_internal.should be_nil
  end
end
