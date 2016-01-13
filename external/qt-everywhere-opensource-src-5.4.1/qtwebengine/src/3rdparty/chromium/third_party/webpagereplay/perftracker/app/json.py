# Copyright 2010 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import datetime
import models
import time

from google.appengine.api import users
from google.appengine.ext import db

from django.utils import simplejson

# GqlEncoder from
# http://stackoverflow.com/questions/2114659/how-to-serialize-db-model-objects-to-json
class GqlEncoder(simplejson.JSONEncoder): 

   """Extends JSONEncoder to add support for GQL results and properties. 

   Adds support to simplejson JSONEncoders for GQL results and properties by 
   overriding JSONEncoder's default method. 
   """ 

   # TODO Improve coverage for all of App Engine's Property types. 

   # When an object contains db.ReferenceProperty attributes, we have the
   # choice to either jsonify the entire ReferenceProperty object recursively
   # or to simply jsonify the key of the referenced property.  By default,
   # only jsonify the keys.
   keys_only = True

   def default(self, obj): 

     """Tests the input object, obj, to encode as JSON.""" 

     if hasattr(obj, '__json__'): 
       return getattr(obj, '__json__')() 

     if isinstance(obj, db.GqlQuery): 
       return list(obj) 

     elif isinstance(obj, db.Model): 
       properties = obj.properties().items() 
       output = {} 
       for field, value in properties: 
         if isinstance(value, db.ReferenceProperty) and self.keys_only:
           # The ReferenceProperty has a method "_attr_name()", which returns
           # the name of the attribute on this object which contains the key
           # to the referenced object.
           # It is important to use this technique to avoid accidentally
           # fetching the actual object.
           key_field_name = value._attr_name()
           key_field = getattr(obj, key_field_name)
           if key_field == None:
             output[field] = "null"
           else:
             output[field] = key_field.id()
         else:
           output[field] = getattr(obj, field) 
       output['key'] = str(getattr(getattr(obj, "key")(), "id")())
       return output 

     elif isinstance(obj, datetime.datetime): 
       return getattr(obj, 'ctime')()

     elif isinstance(obj, time.struct_time): 
       return list(obj) 

     elif isinstance(obj, users.User): 
       output = {} 
       methods = ['nickname', 'email', 'auth_domain'] 
       for method in methods: 
         output[method] = getattr(obj, method)() 
       return output 

     return simplejson.JSONEncoder.default(self, obj) 


def encode(input, keys_only = True): 
   """Encode an input GQL object as JSON 

     Args: 
       input: A GQL object or DB property. 
       keys_only: Don't expand db.ReferenceProperty objects

     Returns: 
       A JSON string based on the input object.  

     Raises: 
       TypeError: Typically occurs when an input object contains an unsupported 
         type. 
     """ 
   encoder = GqlEncoder()
   encoder.keys_only = keys_only
   return encoder.encode(input)
