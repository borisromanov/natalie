require_relative '../../spec_helper'
require_relative 'fixtures/common'

describe "BasicObject" do
  it "raises NoMethodError for nonexistent methods after #method_missing is removed" do
    script = fixture __FILE__, "remove_method_missing.rb"
    ruby_exe(script).chomp.should == "NoMethodError"
  end

  # NATFIXME: raises NameError when referencing built-in constants
  xit "raises NameError when referencing built-in constants" do
    -> { class BasicObjectSpecs::BOSubclass; Kernel; end }.should raise_error(NameError)
  end

  # NATFIXME: does not define built-in constants (according to const_defined?)
  xit "does not define built-in constants (according to const_defined?)" do
    BasicObject.const_defined?(:Kernel).should be_false
  end

  # NATFIXME: does not define built-in constants (according to defined?)
  xit "does not define built-in constants (according to defined?)" do
    BasicObjectSpecs::BOSubclass.kernel_defined?.should be_nil
  end

  it "is included in Object's list of constants" do
    Object.constants(false).should include(:BasicObject)
  end

  # NATFIXME: includes itself in its list of constants
  xit "includes itself in its list of constants" do
    BasicObject.constants(false).should include(:BasicObject)
  end
end

describe "BasicObject metaclass" do
  before :each do
    @meta = class << BasicObject; self; end
  end

  it "is an instance of Class" do
    @meta.should be_an_instance_of(Class)
  end

  it "has Class as superclass" do
    @meta.superclass.should equal(Class)
  end

  it "contains methods for the BasicObject class" do
    @meta.class_eval do
      def rubyspec_test_method() :test end
    end

    BasicObject.rubyspec_test_method.should == :test
  end
end

describe "BasicObject instance metaclass" do
  before :each do
    @object = BasicObject.new
    @meta = class << @object; self; end
  end

  it "is an instance of Class" do
    @meta.should be_an_instance_of(Class)
  end

  it "has BasicObject as superclass" do
    @meta.superclass.should equal(BasicObject)
  end

  it "contains methods defined for the BasicObject instance" do
    @meta.class_eval do
      def test_method() :test end
    end

    @object.test_method.should == :test
  end
end

describe "BasicObject subclass" do
  it "contains Kernel methods when including Kernel" do
    obj = BasicObjectSpecs::BOSubclass.new

    obj.instance_variable_set(:@test, :value)
    obj.instance_variable_get(:@test).should == :value

    obj.respond_to?(:hash).should == true
  end

  describe "BasicObject references" do
    # NATFIXME: uninitialized constant BasicObject::BasicObject (NameError)
    xit "can refer to BasicObject from within itself" do
      -> { BasicObject::BasicObject }.should_not raise_error
    end
  end
end
