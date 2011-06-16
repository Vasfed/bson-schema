require 'spechelper.rb'

describe 'Bson-Schema' do

  it 'validates' do
    Bson::Schema.validate({:properties => {:a => {:type => 'integer'}}}, {:a => 1}).should == true
    Bson::Schema.validate({:properties => {:a => {:type => 'integer'}}}, {:a => 'not int'}).should == false
  end


  it 'validates with exceptions' do
    Bson::Schema.validate!({:properties => {:a => {:type => 'integer'}}}, {:a => 1})

    lambda{
      Bson::Schema.validate!({:properties => {:a => {:type => 'integer'}}}, {:a => 'not int'})
      }.should raise_error

  end


end