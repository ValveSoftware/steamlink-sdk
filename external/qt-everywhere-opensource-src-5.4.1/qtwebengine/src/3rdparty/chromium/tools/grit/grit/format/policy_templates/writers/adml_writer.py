#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from xml.dom import minidom
from grit.format.policy_templates.writers import xml_formatted_writer


def GetWriter(config):
  '''Factory method for instanciating the ADMLWriter. Every Writer needs a
  GetWriter method because the TemplateFormatter uses this method to
  instantiate a Writer.
  '''
  return ADMLWriter(['win'], config)


class ADMLWriter(xml_formatted_writer.XMLFormattedWriter):
  ''' Class for generating an ADML policy template. It is used by the
  PolicyTemplateGenerator to write the ADML file.
  '''

  # DOM root node of the generated ADML document.
  _doc = None

  # The string-table contains all ADML "string" elements.
  _string_table_elem = None

  # The presentation-table is the container for presentation elements, that
  # describe the presentation of Policy-Groups and Policies.
  _presentation_table_elem = None

  def _AddString(self, parent, id, text):
    ''' Adds an ADML "string" element to the passed parent. The following
    ADML snippet contains an example:

    <string id="$(id)">$(text)</string>

    Args:
      parent: Parent element to which the new "string" element is added.
      id: ID of the newly created "string" element.
      text: Value of the newly created "string" element.
    '''
    string_elem = self.AddElement(parent, 'string', {'id': id})
    string_elem.appendChild(self._doc.createTextNode(text))

  def WritePolicy(self, policy):
    '''Generates the ADML elements for a Policy.
    <stringTable>
      ...
      <string id="$(policy_group_name)">$(caption)</string>
      <string id="$(policy_group_name)_Explain">$(description)</string>
    </stringTable>

    <presentationTables>
      ...
      <presentation id=$(policy_group_name)/>
    </presentationTables>

    Args:
      policy: The Policy to generate ADML elements for.
    '''
    policy_type = policy['type']
    policy_name = policy['name']
    if 'caption' in policy:
      policy_caption = policy['caption']
    else:
      policy_caption = policy_name
    if 'desc' in policy:
      policy_description = policy['desc']
    else:
      policy_description = policy_name
    if 'label' in policy:
      policy_label = policy['label']
    else:
      policy_label = policy_name

    self._AddString(self._string_table_elem, policy_name, policy_caption)
    self._AddString(self._string_table_elem, policy_name + '_Explain',
                    policy_description)
    presentation_elem = self.AddElement(
        self._presentation_table_elem, 'presentation', {'id': policy_name})

    if policy_type == 'main':
      pass
    elif policy_type in ('string', 'dict'):
      # 'dict' policies are configured as JSON-encoded strings on Windows.
      textbox_elem = self.AddElement(presentation_elem, 'textBox',
                                     {'refId': policy_name})
      label_elem = self.AddElement(textbox_elem, 'label')
      label_elem.appendChild(self._doc.createTextNode(policy_label))
    elif policy_type == 'int':
      textbox_elem = self.AddElement(presentation_elem, 'decimalTextBox',
                                     {'refId': policy_name})
      textbox_elem.appendChild(self._doc.createTextNode(policy_label + ':'))
    elif policy_type in ('int-enum', 'string-enum'):
      for item in policy['items']:
        self._AddString(self._string_table_elem, item['name'], item['caption'])
      dropdownlist_elem = self.AddElement(presentation_elem, 'dropdownList',
                                          {'refId': policy_name})
      dropdownlist_elem.appendChild(self._doc.createTextNode(policy_label))
    elif policy_type == 'list':
      self._AddString(self._string_table_elem,
                      policy_name + 'Desc',
                      policy_caption)
      listbox_elem = self.AddElement(presentation_elem, 'listBox',
                                     {'refId': policy_name + 'Desc'})
      listbox_elem.appendChild(self._doc.createTextNode(policy_label))
    elif policy_type == 'group':
      pass
    elif policy_type == 'external':
      # This type can only be set through cloud policy.
      pass
    else:
      raise Exception('Unknown policy type %s.' % policy_type)

  def BeginPolicyGroup(self, group):
    '''Generates ADML elements for a Policy-Group. For each Policy-Group two
    ADML "string" elements are added to the string-table. One contains the
    caption of the Policy-Group and the other a description. A Policy-Group also
    requires an ADML "presentation" element that must be added to the
    presentation-table. The "presentation" element is the container for the
    elements that define the visual presentation of the Policy-Goup's Policies.
    The following ADML snippet shows an example:

    Args:
      group: The Policy-Group to generate ADML elements for.
    '''
    # Add ADML "string" elements to the string-table that are required by a
    # Policy-Group.
    self._AddString(self._string_table_elem, group['name'] + '_group',
                    group['caption'])

  def _AddBaseStrings(self, string_table_elem, build):
    ''' Adds ADML "string" elements to the string-table that are referenced by
    the ADMX file but not related to any specific Policy-Group or Policy.
    '''
    self._AddString(string_table_elem, self.config['win_supported_os'],
                    self.messages['win_supported_winxpsp2']['text'])
    recommended_name = '%s - %s' % \
        (self.config['app_name'], self.messages['doc_recommended']['text'])
    if build == 'chrome':
      self._AddString(string_table_elem,
                      self.config['win_mandatory_category_path'][0],
                      'Google')
      self._AddString(string_table_elem,
                      self.config['win_mandatory_category_path'][1],
                      self.config['app_name'])
      self._AddString(string_table_elem,
                      self.config['win_recommended_category_path'][1],
                      recommended_name)
    elif build == 'chromium':
      self._AddString(string_table_elem,
                      self.config['win_mandatory_category_path'][0],
                      self.config['app_name'])
      self._AddString(string_table_elem,
                      self.config['win_recommended_category_path'][0],
                      recommended_name)

  def BeginTemplate(self):
    dom_impl = minidom.getDOMImplementation('')
    self._doc = dom_impl.createDocument(None, 'policyDefinitionResources',
                                        None)
    policy_definitions_resources_elem = self._doc.documentElement
    policy_definitions_resources_elem.attributes['revision'] = '1.0'
    policy_definitions_resources_elem.attributes['schemaVersion'] = '1.0'

    self.AddElement(policy_definitions_resources_elem, 'displayName')
    self.AddElement(policy_definitions_resources_elem, 'description')
    resources_elem = self.AddElement(policy_definitions_resources_elem,
                                     'resources')
    self._string_table_elem = self.AddElement(resources_elem, 'stringTable')
    self._AddBaseStrings(self._string_table_elem, self.config['build'])
    self._presentation_table_elem = self.AddElement(resources_elem,
                                                   'presentationTable')

  def GetTemplateText(self):
    # Using "toprettyxml()" confuses the Windows Group Policy Editor
    # (gpedit.msc) because it interprets whitespace characters in text between
    # the "string" tags. This prevents gpedit.msc from displaying the category
    # names correctly.
    # TODO(markusheintz): Find a better formatting that works with gpedit.
    return self._doc.toxml()
